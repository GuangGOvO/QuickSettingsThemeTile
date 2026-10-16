[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$DllPath,
    [string]$ScannerPath = '.\compat-check-033\compat-scan.exe',
    [string]$InspectorPath = '.\compat-pdb-tools\compat-pdb-inspect.exe',
    [string]$DiaPath,
    [string]$Filter = 'PaginatedToggleButton|QuickAction|Theme|Toggle',
    [string]$OutputDirectory = '.compat-index'
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($Filter)) { throw 'Filter must be a focused symbol-name substring or list.' }
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
function Resolve-InputPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return (Resolve-Path -LiteralPath $Path).Path }
    return (Resolve-Path -LiteralPath (Join-Path (Get-Location).Path $Path)).Path
}

$dll = Resolve-InputPath $DllPath
$scanner = Resolve-InputPath $ScannerPath
$inspector = Resolve-InputPath $InspectorPath
if ([string]::IsNullOrWhiteSpace($DiaPath)) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) { throw 'vswhere.exe was not found; pass -DiaPath to msdia140.dll.' }
    $vs = & $vswhere -latest -products '*' -property installationPath
    if (!$vs) { throw 'Visual Studio was not found; pass -DiaPath to msdia140.dll.' }
    $DiaPath = Join-Path $vs 'Common7\IDE\msdia140.dll'
}
$dia = Resolve-InputPath $DiaPath

$scannerOutput = @(& $scanner $dll 2>$null)
$scannerExitCode = $LASTEXITCODE
if ($scannerExitCode -notin 0,2) { throw "Compatibility scanner failed with exit code $scannerExitCode." }
$scan = ($scannerOutput -join "`n") | ConvertFrom-Json
if ($scan.status -notin 'supported','unknown_binary') { throw "Candidate scanner returned $($scan.status)." }
if (!$scan.pdbGuid -or !$scan.pdbAge -or !$scan.sha256) { throw 'DLL has no complete CodeView PDB identity.' }

[byte[]]$dllBytes = [IO.File]::ReadAllBytes($dll)
$ascii = [Text.Encoding]::ASCII.GetString($dllBytes)
$pdbMatches = [regex]::Matches($ascii,'RSDS[\s\S]{20}(?<path>[^\x00]{1,260}?\.pdb)\x00',[Text.RegularExpressions.RegexOptions]::IgnoreCase)
$pdbName = $null
foreach ($match in $pdbMatches) {
    $candidateName = ($match.Groups['path'].Value -split '[\\/]')[-1]
    if ($candidateName -match '^[A-Za-z0-9_.-]+\.pdb$') { $pdbName = $candidateName; break }
}
if (!$pdbName) { throw 'Could not read the PDB basename from the DLL CodeView record.' }

$outputRoot = if ([IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $repoRoot $OutputDirectory }
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$hash = $scan.sha256.ToLowerInvariant()
$pdbPath = Join-Path $outputRoot "pdb-$hash.pdb"
$guid = [guid]::Parse([string]$scan.pdbGuid)
$age = [uint32]$scan.pdbAge
$symbolKey = $guid.ToString('N').ToUpperInvariant() + $age.ToString('X')
$escapedName = [uri]::EscapeDataString($pdbName)
$symbolUri = "https://msdl.microsoft.com/download/symbols/$escapedName/$symbolKey/$escapedName"

$evidence = Join-Path $outputRoot "pdb-review-$hash.tsv"
$diagnostics = Join-Path $outputRoot "pdb-review-$hash.stderr.txt"
$guidText = $guid.ToString('B')
function Invoke-PdbInspection([string]$Path) {
    & $inspector $dia $Path $guidText ([string]$age) $Filter 1> $evidence 2> $diagnostics
    return $LASTEXITCODE
}
$cachedPdb = Test-Path -LiteralPath $pdbPath
if ($cachedPdb) {
    Write-Verbose 'Checking cached PDB identity.'
    $inspectExitCode = Invoke-PdbInspection $pdbPath
} else {
    Write-Verbose 'Downloading exact PDB identity from the Microsoft public symbol server.'
    Invoke-WebRequest -Uri $symbolUri -OutFile $pdbPath -TimeoutSec 300 -ErrorAction Stop
    $inspectExitCode = Invoke-PdbInspection $pdbPath
}
if ($inspectExitCode -notin 0,3) {
    $details = if (Test-Path -LiteralPath $diagnostics) { (Get-Content -LiteralPath $diagnostics -Raw).Trim() } else { '' }
    $stage = if ($cachedPdb) { 'Cached PDB identity check' } else { 'Downloaded PDB identity check' }
    throw "$stage failed with exit code $inspectExitCode. $details"
}
$inspectionText = Get-Content -LiteralPath $diagnostics -Raw
$counts = [regex]::Match($inspectionText,'matchedTypes=(\d+) matchedPublics=(\d+) matchedFunctions=(\d+)')
if (!$counts.Success) { throw 'PDB inspector did not return its symbol counts.' }

[pscustomobject]@{
    CandidateSha256 = $hash
    CandidateStatus = [string]$scan.status
    PdbIdentityVerified = $true
    PdbGuid = $guid.ToString('B').ToUpperInvariant()
    PdbAge = $age
    PdbBytes = (Get-Item -LiteralPath $pdbPath).Length
    MatchingUdtCount = [int]$counts.Groups[1].Value
    MatchingPublicSymbolCount = [int]$counts.Groups[2].Value
    MatchingFunctionCount = [int]$counts.Groups[3].Value
    EvidenceFile = [IO.Path]::GetRelativePath($repoRoot,$evidence)
    ReviewBoundary = 'public symbol names only; this output does not prove ABI compatibility or add a supported profile'
}
