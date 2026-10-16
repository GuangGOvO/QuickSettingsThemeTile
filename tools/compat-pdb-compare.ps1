[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$ReferencePdb,
    [Parameter(Mandatory=$true)][string]$ReferenceGuid,
    [Parameter(Mandatory=$true)][uint32]$ReferenceAge,
    [Parameter(Mandatory=$true)][string]$CandidatePdb,
    [Parameter(Mandatory=$true)][string]$CandidateGuid,
    [Parameter(Mandatory=$true)][uint32]$CandidateAge,
    [string]$InspectorPath = '.\compat-pdb-tools\compat-pdb-inspect.exe',
    [string]$DiaPath,
    [string[]]$Filters = @('PaginatedToggleButton','IPaginatedToggleButton','IQuickAction'),
    [string]$OutputDirectory = '.compat-index'
)

$ErrorActionPreference = 'Stop'
function Resolve-InputPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return (Resolve-Path -LiteralPath $Path).Path }
    return (Resolve-Path -LiteralPath (Join-Path (Get-Location).Path $Path)).Path
}

$reference = Resolve-InputPath $ReferencePdb
$candidate = Resolve-InputPath $CandidatePdb
$inspector = Resolve-InputPath $InspectorPath
if ([string]::IsNullOrWhiteSpace($DiaPath)) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) { throw 'vswhere.exe was not found; pass -DiaPath to msdia140.dll.' }
    $vs = & $vswhere -latest -products '*' -property installationPath
    if (!$vs) { throw 'Visual Studio was not found; pass -DiaPath to msdia140.dll.' }
    $DiaPath = Join-Path $vs 'Common7\IDE\msdia140.dll'
}
$dia = Resolve-InputPath $DiaPath
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$outputRoot = if ([IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $repoRoot $OutputDirectory }
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null

$hashInput = "$ReferenceGuid|$ReferenceAge|$CandidateGuid|$CandidateAge|$($Filters -join '|')"
$suffix = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($hashInput))).Substring(0,12).ToLowerInvariant()
$rows = foreach ($filter in $Filters) {
    if ([string]::IsNullOrWhiteSpace($filter) -or $filter.Contains('|')) { throw 'Each filter must be a non-empty substring without |.' }
    $safeFilter = $filter -replace '[^A-Za-z0-9_.-]','_'
    $referenceOutput = Join-Path $outputRoot "pdb-compare-$suffix-$safeFilter-reference.tsv"
    $referenceErrors = Join-Path $outputRoot "pdb-compare-$suffix-$safeFilter-reference.stderr.txt"
    $candidateOutput = Join-Path $outputRoot "pdb-compare-$suffix-$safeFilter-candidate.tsv"
    $candidateErrors = Join-Path $outputRoot "pdb-compare-$suffix-$safeFilter-candidate.stderr.txt"
    & $inspector $dia $reference ([guid]::Parse($ReferenceGuid).ToString('B')) ([string]$ReferenceAge) $filter 1> $referenceOutput 2> $referenceErrors
    $referenceExit = $LASTEXITCODE
    & $inspector $dia $candidate ([guid]::Parse($CandidateGuid).ToString('B')) ([string]$CandidateAge) $filter 1> $candidateOutput 2> $candidateErrors
    $candidateExit = $LASTEXITCODE
    if ($referenceExit -notin 0,3) { throw "Reference PDB validation failed: $((Get-Content -LiteralPath $referenceErrors -Raw).Trim())" }
    if ($candidateExit -notin 0,3) { throw "Candidate PDB validation failed: $((Get-Content -LiteralPath $candidateErrors -Raw).Trim())" }
    $referenceCounts = [regex]::Match((Get-Content -LiteralPath $referenceErrors -Raw),'matchedTypes=(\d+) matchedPublics=(\d+) matchedFunctions=(\d+)')
    $candidateCounts = [regex]::Match((Get-Content -LiteralPath $candidateErrors -Raw),'matchedTypes=(\d+) matchedPublics=(\d+) matchedFunctions=(\d+)')
    if (!$referenceCounts.Success -or !$candidateCounts.Success) { throw 'PDB inspector did not return symbol counts for both inputs.' }

    $referenceSymbols = @(Get-Content -LiteralPath $referenceOutput -ErrorAction SilentlyContinue | ForEach-Object { $_ -replace '\t[0-9]+$','' } | Sort-Object -Unique -CaseSensitive)
    $candidateSymbols = @(Get-Content -LiteralPath $candidateOutput -ErrorAction SilentlyContinue | ForEach-Object { $_ -replace '\t[0-9]+$','' } | Sort-Object -Unique -CaseSensitive)
    $delta = @(Compare-Object -ReferenceObject $referenceSymbols -DifferenceObject $candidateSymbols -CaseSensitive)
    $diffRows = foreach ($item in $delta) {
        [pscustomobject]@{Filter=$filter;Change=if($item.SideIndicator -eq '<='){'reference_only'}else{'candidate_only'};Symbol=$item.InputObject}
    }
    $diffPath = Join-Path $outputRoot "pdb-compare-$suffix-$safeFilter-diff.tsv"
    if ($diffRows) { $diffRows | Export-Csv -LiteralPath $diffPath -NoTypeInformation -Delimiter "`t" -Encoding utf8 }
    [pscustomobject]@{
        Filter = $filter
        ReferenceUdtCount = [int]$referenceCounts.Groups[1].Value
        CandidateUdtCount = [int]$candidateCounts.Groups[1].Value
        ReferencePublicSymbolCount = [int]$referenceCounts.Groups[2].Value
        CandidatePublicSymbolCount = [int]$candidateCounts.Groups[2].Value
        ReferenceFunctionCount = [int]$referenceCounts.Groups[3].Value
        CandidateFunctionCount = [int]$candidateCounts.Groups[3].Value
        ReferenceSymbolCount = $referenceSymbols.Count
        CandidateSymbolCount = $candidateSymbols.Count
        ReferenceOnlyCount = @($delta | Where-Object SideIndicator -eq '<=').Count
        CandidateOnlyCount = @($delta | Where-Object SideIndicator -eq '=>').Count
        SamePublicSymbolNames = ($delta.Count -eq 0)
        DiffFile = if ($delta.Count) { [IO.Path]::GetRelativePath($repoRoot,$diffPath) } else { '' }
    }
}

$summaryPath = Join-Path $outputRoot "pdb-compare-$suffix-summary.json"
$report = [pscustomobject]@{
    SchemaVersion = 1
    GeneratedAtUtc = [DateTime]::UtcNow.ToString('o')
    ReferenceGuid = ([guid]::Parse($ReferenceGuid)).ToString('B').ToUpperInvariant()
    ReferenceAge = $ReferenceAge
    CandidateGuid = ([guid]::Parse($CandidateGuid)).ToString('B').ToUpperInvariant()
    CandidateAge = $CandidateAge
    Filters = @($rows)
    ReviewBoundary = 'a matching public symbol-name set is a triage signal only; it does not prove type layout, ABI safety, or host compatibility'
}
[IO.File]::WriteAllText($summaryPath,($report | ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
$report
