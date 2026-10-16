[CmdletBinding()]
param(
    [string]$SummaryPath = '.compat-index\scan-all-summary.json',
    [string]$ScannerPath = '.\compat-check-034\compat-scan.exe',
    [string]$ProbePath = '.\compat-abi-probe\compat-abi-probe.exe',
    [string]$InspectorPath = '.\compat-pdb-tools\compat-pdb-inspect.exe',
    [string]$OutputDirectory = '.compat-index',
    [string[]]$CandidateHash,
    [switch]$AllUnknown
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path

function Resolve-RepoPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return (Resolve-Path -LiteralPath $Path).Path }
    return (Resolve-Path -LiteralPath (Join-Path $repoRoot $Path)).Path
}

if (!$CandidateHash -and !$AllUnknown) {
    throw 'Pass one or more -CandidateHash values, or explicitly pass -AllUnknown.'
}
$summaryFile = Resolve-RepoPath $SummaryPath
$scanner = Resolve-RepoPath $ScannerPath
$probe = Resolve-RepoPath $ProbePath
$inspector = Resolve-RepoPath $InspectorPath
$summary = Get-Content -Raw -LiteralPath $summaryFile | ConvertFrom-Json
$candidateRecords = @{}
foreach ($entry in $summary.results) {
    $candidateRecords[[string]$entry.Sha256.ToLowerInvariant()] = [string]$entry.Report
}

if ($AllUnknown) {
    $selectedHashes = @($summary.results | Where-Object Outcome -eq 'unknown_binary' | ForEach-Object { [string]$_.Sha256.ToLowerInvariant() })
} else {
    $selectedHashes = @($CandidateHash | ForEach-Object { $_.ToLowerInvariant() } | Select-Object -Unique)
}
if (!$selectedHashes.Count) { throw 'No candidate hashes were selected.' }

$outRoot = if ([IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $repoRoot $OutputDirectory }
New-Item -ItemType Directory -Path $outRoot -Force | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'vswhere.exe was not found; install Visual Studio DIA tools or pass the DIA path through PATH.' }
$vs = & $vswhere -latest -products '*' -property installationPath
if (!$vs) { throw 'Visual Studio installation was not found.' }
$dia = Join-Path $vs 'Common7\IDE\msdia140.dll'
if (!(Test-Path -LiteralPath $dia)) { throw 'Visual Studio DIA SDK msdia140.dll was not found.' }

$results = [Collections.Generic.List[object]]::new()
foreach ($hash in $selectedHashes) {
    if ($hash -notmatch '^[0-9a-f]{64}$' -or !$candidateRecords.ContainsKey($hash)) {
        throw "Hash '$hash' is not present in the selected Winbindex summary."
    }

    $recordPath = Join-Path (Split-Path -Parent $summaryFile) $candidateRecords[$hash]
    $record = Get-Content -Raw -LiteralPath $recordPath | ConvertFrom-Json
    if ([string]$record.sha256 -ne $hash) { throw "Candidate record hash does not match selected hash $hash." }
    $dll = Join-Path (Split-Path -Parent $summaryFile) "downloads\$hash\ControlCenter.dll"
    if (!(Test-Path -LiteralPath $dll)) { throw "Cached DLL for $hash is missing; run compat-discover.ps1 -ScanAll first." }

    $result = [ordered]@{
        sha256 = $hash
        associations = @($record.associations | ForEach-Object { "$($_.WindowsFamily) $($_.ReleaseVersion) $($_.Update)" })
        authenticodeStatus = [string]$record.authenticodeStatus
        codeViewGuid = [string]$record.compatibilityScan.pdbGuid
        codeViewAge = [int]$record.compatibilityScan.pdbAge
        diaReportedGuid = ''
        diaReportedAge = $null
        diaIdentityMatchesCodeView = $false
        pdbInfoStreamIdentityVerified = $false
        initializeRva = $null
        initializeIid = $null
        probeStatus = 'not_run'
        probe = $null
        failureStage = $null
        failure = $null
    }

    try {
        $scanLines = @(& $scanner $dll 2>$null)
        $scanCode = $LASTEXITCODE
        if ($scanCode -notin 0,2) { throw "compat-scan.exe exited with $scanCode." }
        $scan = ($scanLines -join "`n") | ConvertFrom-Json
        if ($scan.sha256.ToLowerInvariant() -ne $hash) { throw 'Cached DLL hash does not match the index record.' }
        if (!$scan.pdbGuid -or !$scan.pdbAge) { throw 'Candidate DLL has no complete PDB identity.' }

        [byte[]]$dllBytes = [IO.File]::ReadAllBytes($dll)
        $ascii = [Text.Encoding]::ASCII.GetString($dllBytes)
        $matches = [regex]::Matches($ascii,'RSDS[\s\S]{20}(?<path>[^\x00]{1,260}?\.pdb)\x00',[Text.RegularExpressions.RegexOptions]::IgnoreCase)
        $pdbName = $null
        foreach ($match in $matches) {
            $name = ($match.Groups['path'].Value -split '[\\/]')[-1]
            if ($name -match '^[A-Za-z0-9_.-]+\.pdb$') { $pdbName = $name; break }
        }
        if (!$pdbName) { throw 'PDB basename could not be read from the DLL CodeView record.' }

        $pdbPath = Join-Path $outRoot "pdb-$hash.pdb"
        if (!(Test-Path -LiteralPath $pdbPath) -or (Get-Item -LiteralPath $pdbPath).Length -eq 0) {
            $guid = [guid]::Parse([string]$scan.pdbGuid)
            $symbolKey = $guid.ToString('N').ToUpperInvariant() + ([uint32]$scan.pdbAge).ToString('X')
            $encodedName = [uri]::EscapeDataString($pdbName)
            $uri = "https://msdl.microsoft.com/download/symbols/$encodedName/$symbolKey/$encodedName"
            $partialPath = "$pdbPath.partial"
            if (Test-Path -LiteralPath $partialPath) { Remove-Item -LiteralPath $partialPath -Force }
            Invoke-WebRequest -Uri $uri -OutFile $partialPath -TimeoutSec 90 -ErrorAction Stop
            if ((Get-Item -LiteralPath $partialPath).Length -lt 1024) { throw 'Downloaded PDB is unexpectedly small.' }
            Move-Item -LiteralPath $partialPath -Destination $pdbPath -Force
        }

        $evidence = Join-Path $outRoot "pdb-review-$hash.tsv"
        $diagnostics = Join-Path $outRoot "pdb-review-$hash.stderr.txt"
        & $inspector $dia $pdbPath ([string]$scan.pdbGuid) ([string]$scan.pdbAge) 'IQuickActionViewModel' 1> $evidence 2> $diagnostics
        $inspectCode = $LASTEXITCODE
        if ($inspectCode -notin 0,3) { throw "DIA identity/symbol inspection failed with exit code $inspectCode." }
        $diagnosticText = Get-Content -Raw -LiteralPath $diagnostics
        $identity = [regex]::Match($diagnosticText,'diaGuid=\{(?<guid>[0-9A-Fa-f-]+)\} diaAge=(?<age>\d+)')
        if (!$identity.Success -or ([guid]$identity.Groups['guid'].Value -ne [guid]$scan.pdbGuid) -or ([int]$identity.Groups['age'].Value -ne [int]$scan.pdbAge)) {
            throw 'DIA-reported GUID/age did not match the candidate DLL CodeView key.'
        }
        $result.diaReportedGuid = ([guid]$identity.Groups['guid'].Value).ToString('B').ToUpperInvariant()
        $result.diaReportedAge = [int]$identity.Groups['age'].Value
        $result.diaIdentityMatchesCodeView = $true

        $rvaMatches = @(Get-Content -LiteralPath $evidence | ForEach-Object {
            if ($_ -match '^PUBLIC\t.*produce<.*QuickActionViewModel.*IQuickActionViewModel.*>::Initialize\(void\).*\t(?<rva>\d+)$') { [uint32]$Matches.rva }
        } | Select-Object -Unique)
        if ($rvaMatches.Count -ne 1) { throw "Expected one IQuickActionViewModel::Initialize implementation symbol; found $($rvaMatches.Count)." }
        $rva = [uint32]$rvaMatches[0]
        $result.initializeRva = ('0x{0:X}' -f $rva)

        $probeLines = @(& $probe $dll ('0x{0:X}' -f $rva) 2>$null)
        $probeCode = $LASTEXITCODE
        $probeData = ($probeLines -join "`n") | ConvertFrom-Json
        $result.probe = $probeData
        $result.probeStatus = [string]$probeData.probeStatus
        $result.initializeIid = [string]$probeData.matchedInitializeIid
        if ([int]$probeData.matchedInitializeIidCount -ne 1 -or !$result.initializeIid) {
            throw 'The candidate did not expose one unambiguous Initialize interface IID.'
        }
        if ($probeCode -ne 0 -or $probeData.probeStatus -ne 'abi_contract_match') {
            throw "Isolated ABI probe did not pass (exit $probeCode; $($probeData.probeStatus))."
        }
    } catch {
        $result.failureStage = if (!$result.diaIdentityMatchesCodeView) { 'symbol_or_candidate_validation' } elseif (!$result.initializeRva) { 'initialize_symbol' } else { 'isolated_abi_probe' }
        $result.failure = $_.Exception.Message
        if ($result.probeStatus -eq 'not_run') { $result.probeStatus = 'inconclusive' }
    }
    $results.Add([pscustomobject]$result)
    Write-Host ("{0}  {1}  {2}" -f $hash.Substring(0,12),$result.probeStatus,($result.initializeRva ?? $result.failure))
}

$report = [ordered]@{
    schemaVersion = 1
    generatedAtUtc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
    sourceSummary = [IO.Path]::GetFileName($summaryFile)
    boundary = 'DIA-reported GUID/age is not a parse of the canonical PDB Info stream. Candidate screening only: a direct binary ABI probe is required, no profile table or Shell configuration is changed, and target-build Shell UI acceptance remains separate.'
    results = @($results)
}
$reportPath = Join-Path $outRoot 'abi-batch-review.json'
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath -Encoding utf8
Write-Host "Report: $([IO.Path]::GetRelativePath($repoRoot,$reportPath))"
