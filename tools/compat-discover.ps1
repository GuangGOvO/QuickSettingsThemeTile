#requires -Version 7.0
[CmdletBinding()]
param(
    [ValidatePattern('^\d+\.\d+$')]
    [string]$Build,
    [string]$OutputDirectory,
    [string]$ScannerPath,
    [switch]$Download,
    [switch]$ScanAll,
    [string]$DllPath,
    [string]$DllDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$indexUri = 'https://winbindex.m417z.com/data/by_filename_compressed/controlcenter.dll.json.gz'
$indexPage = 'https://winbindex.m417z.com/?file=controlcenter.dll'
$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot '.compat-index'
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)

if ($Download -and [string]::IsNullOrWhiteSpace($Build)) {
    throw 'Use -Build with -Download so only one explicitly selected build is fetched.'
}
if ($ScanAll -and $Build) {
    throw 'Omit -Build with -ScanAll; it scans each unique indexed hash.'
}
if (($Download -or $ScanAll) -and ($DllPath -or $DllDirectory)) {
    throw 'Use either an indexed download mode or a local DLL input, not both.'
}
if (($Download -or $ScanAll -or $DllPath -or $DllDirectory) -and [string]::IsNullOrWhiteSpace($ScannerPath)) {
    throw 'Use -ScannerPath when downloading or scanning DLL files.'
}
if ($DllPath -and $DllDirectory) {
    throw 'Use either -DllPath or -DllDirectory, not both.'
}
if ($Download -or $ScanAll -or $DllPath -or $DllDirectory) {
    $ScannerPath = (Resolve-Path -LiteralPath $ScannerPath).Path
}

function Get-HttpBytes {
    param(
        [Parameter(Mandatory)][System.Net.Http.HttpClient]$Client,
        [Parameter(Mandatory)][string]$Uri
    )
    return $Client.GetByteArrayAsync($Uri).GetAwaiter().GetResult()
}

function ConvertTo-HexValue {
    param([Parameter(Mandatory)][uint64]$Value)
    return ('0x{0:X}' -f $Value)
}

function Invoke-CompatibilityScanner {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Scanner
    )

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Scanner
    [void]$startInfo.ArgumentList.Add($Path)
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [System.Diagnostics.Process]::Start($startInfo)
    if ($null -eq $process) { throw 'Could not start compat-scan.exe.' }
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    if ($process.ExitCode -notin 0, 2) {
        throw "compat-scan.exe exited with code $($process.ExitCode): $stderr"
    }
    if ([string]::IsNullOrWhiteSpace($stdout)) {
        throw 'compat-scan.exe returned no JSON.'
    }
    return [pscustomobject]@{
        ExitCode = $process.ExitCode
        Result = ($stdout | ConvertFrom-Json)
    }
}

if ($DllPath -or $DllDirectory) {
    if ($DllPath) {
        $inputFile = Get-Item -LiteralPath $DllPath -ErrorAction Stop
        if ($inputFile.PSIsContainer) { throw '-DllPath must name a file.' }
        $inputFiles = @($inputFile)
    } else {
        $inputDirectory = Get-Item -LiteralPath $DllDirectory -ErrorAction Stop
        if (-not $inputDirectory.PSIsContainer) { throw '-DllDirectory must name a directory.' }
        $inputFiles = @(Get-ChildItem -LiteralPath $inputDirectory.FullName -Filter 'ControlCenter.dll' -File -Recurse)
    }
    if ($inputFiles.Count -eq 0) { throw 'No ControlCenter.dll files were found in the selected input.' }

    $localResults = [System.Collections.Generic.List[object]]::new()
    foreach ($inputFile in $inputFiles) {
        $hash = (Get-FileHash -LiteralPath $inputFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        $signature = Get-AuthenticodeSignature -FilePath $inputFile.FullName
        $scan = Invoke-CompatibilityScanner -Path $inputFile.FullName -Scanner $ScannerPath
        $buildSlug = if ($Build) { $Build.Replace('.', '-') } else { 'unlabelled' }
        $reportName = "scan-local-$buildSlug-$hash.json"
        $localReport = [ordered]@{
            schemaVersion = 1
            generatedAtUtc = [DateTime]::UtcNow.ToString('O')
            source = 'caller-supplied local ControlCenter.dll; source build label is not verified by this scanner'
            sourceBuildLabel = $Build
            sourceBuildLabelVerified = $false
            inputPathRecorded = $false
            fileName = [string]$inputFile.Name
            fileLength = [long]$inputFile.Length
            fileVersion = [string]$inputFile.VersionInfo.FileVersion
            sha256 = $hash
            authenticodeStatus = [string]$signature.Status
            authenticodeCheckScope = 'scanner host signature lookup; no source package catalog was supplied for separate verification'
            authenticodeSigner = if ($signature.SignerCertificate) { [string]$signature.SignerCertificate.Subject } else { '' }
            compatibilityScanExitCode = $scan.ExitCode
            compatibilityScan = $scan.Result
            reviewState = 'candidate-only; not added to the profile table and never loaded into Shell'
        }
        Set-Content -LiteralPath (Join-Path $OutputDirectory $reportName) -Value (ConvertTo-Json -InputObject $localReport -Depth 12) -Encoding utf8NoBOM
        $localResults.Add([pscustomobject]@{
            Sha256 = $hash
            FileVersion = [string]$inputFile.VersionInfo.FileVersion
            Status = [string]$scan.Result.status
            Supported = [bool]$scan.Result.supported
            ProfileId = [string]$scan.Result.profileId
            AuthenticodeStatus = [string]$signature.Status
            Report = $reportName
        })
    }

    [pscustomobject]@{
        mode = 'local-scan'
        requestedBuildLabel = $Build
        candidateFileCount = $localResults.Count
        scanResults = @($localResults)
        localPathsRecorded = $false
        profileTableChanged = $false
        shellLoaded = $false
    } | ConvertTo-Json -Depth 8 -Compress
    return
}

$client = [System.Net.Http.HttpClient]::new()
$client.Timeout = [TimeSpan]::FromSeconds(60)
try {
    $compressed = Get-HttpBytes -Client $client -Uri $indexUri
    $inputStream = [System.IO.MemoryStream]::new($compressed)
    $gzip = [System.IO.Compression.GZipStream]::new(
        $inputStream,
        [System.IO.Compression.CompressionMode]::Decompress
    )
    $reader = [System.IO.StreamReader]::new($gzip)
    try {
        $indexJson = $reader.ReadToEnd()
    } finally {
        $reader.Dispose()
        $gzip.Dispose()
        $inputStream.Dispose()
    }

    $index = ConvertFrom-Json -InputObject $indexJson -AsHashtable
    if ($index -isnot [System.Collections.IDictionary]) {
        throw 'Winbindex returned an unexpected data format.'
    }

    $records = [System.Collections.Generic.List[object]]::new()
    foreach ($hash in $index.Keys) {
        if ($hash -notmatch '^[0-9a-fA-F]{64}$') { continue }
        $entry = $index[$hash]
        if ($entry -isnot [System.Collections.IDictionary] -or -not $entry.Contains('fileInfo')) { continue }
        $fileInfo = $entry['fileInfo']
        if ($fileInfo -isnot [System.Collections.IDictionary] -or -not $fileInfo.Contains('machineType')) { continue }
        if ([uint32]$fileInfo['machineType'] -ne 0x8664) { continue }
        if (-not $entry.Contains('windowsVersions') -or $entry['windowsVersions'] -isnot [System.Collections.IDictionary]) { continue }

        foreach ($family in $entry['windowsVersions'].Keys) {
            $updates = $entry['windowsVersions'][$family]
            if ($updates -isnot [System.Collections.IDictionary]) { continue }
            foreach ($kb in $updates.Keys) {
                $update = $updates[$kb]
                if ($update -isnot [System.Collections.IDictionary] -or -not $update.Contains('updateInfo') -or -not $update.Contains('assemblies')) { continue }
                $info = $update['updateInfo']
                $assemblies = $update['assemblies']
                if ($info -isnot [System.Collections.IDictionary] -or $assemblies -isnot [System.Collections.IDictionary]) { continue }
                if (-not $info.Contains('releaseVersion')) { continue }
                $releaseVersion = [string]$info['releaseVersion']

                foreach ($assemblyKey in $assemblies.Keys) {
                    $assembly = $assemblies[$assemblyKey]
                    if ($assembly -isnot [System.Collections.IDictionary] -or -not $assembly.Contains('assemblyIdentity')) { continue }
                    $identity = $assembly['assemblyIdentity']
                    if ($null -eq $identity) { continue }
                    if ($identity -isnot [System.Collections.IDictionary] -or -not $identity.Contains('processorArchitecture') -or -not $identity.Contains('name')) { continue }
                    if ([string]$identity['processorArchitecture'] -ne 'amd64') { continue }
                    if ([string]$identity['name'] -ne 'Microsoft-Windows-Shell-ControlCenter') { continue }

                    $heading = if ($info.Contains('heading')) { [string]$info['heading'] } else { '' }
                    $identityVersion = if ($identity.Contains('version')) { [string]$identity['version'] } else { '' }
                    $buildMatchEvidence = [System.Collections.Generic.List[string]]::new()
                    if ($Build) {
                        if ($releaseVersion -match "(?:^|\.)$([regex]::Escape($Build))$") { $buildMatchEvidence.Add('releaseVersion') }
                        if ($heading -match "(?<!\d)$([regex]::Escape($Build))(?!\d)") { $buildMatchEvidence.Add('updateTitle') }
                        if ($identityVersion -match "(?<!\d)$([regex]::Escape($Build))(?!\d)") { $buildMatchEvidence.Add('assemblyIdentity') }
                        if ($buildMatchEvidence.Count -eq 0) { continue }
                    }
                    $timestamp = if ($fileInfo.Contains('timestamp')) { [uint32]$fileInfo['timestamp'] } else { [uint32]0 }
                    $imageSize = if ($fileInfo.Contains('virtualSize')) { [uint32]$fileInfo['virtualSize'] } else { [uint32]0 }
                    $previewTagged = $heading -match '(?i)preview|insider|beta|canary|experimental'
                    $records.Add([pscustomobject]@{
                        Sha256 = $hash.ToLowerInvariant()
                        MachineType = [uint32]$fileInfo['machineType']
                        WinbindexSigningStatus = if ($fileInfo.Contains('signingStatus')) { [string]$fileInfo['signingStatus'] } else { '' }
                        Timestamp = $timestamp
                        ImageSize = $imageSize
                        FileVersion = $identityVersion
                        BuildMatchEvidence = @($buildMatchEvidence)
                        WindowsFamily = [string]$family
                        ReleaseVersion = $releaseVersion
                        Update = [string]$kb
                        UpdateDate = if ($info.Contains('releaseDate')) { [string]$info['releaseDate'] } else { '' }
                        UpdateTitle = $heading
                        UpdateUrl = if ($info.Contains('updateUrl')) { [string]$info['updateUrl'] } else { '' }
                        ProcessorArchitecture = [string]$identity['processorArchitecture']
                        BuildType = if ($identity.Contains('buildType')) { [string]$identity['buildType'] } else { '' }
                        PreviewTagged = [bool]$previewTagged
                    })
                }
            }
        }
    }

    if ($records.Count -eq 0) {
        if ($Build) { throw "Winbindex has no AMD64 ControlCenter.dll entry for build $Build." }
        throw 'Winbindex returned no AMD64 ControlCenter.dll entries.'
    }

    $candidates = @(
        foreach ($group in ($records | Group-Object Sha256 | Sort-Object Name)) {
            $first = $group.Group[0]
            $downloadKey = if ($first.Timestamp -gt 0 -and $first.ImageSize -gt 0) {
                '{0:X8}{1:X}' -f $first.Timestamp, $first.ImageSize
            } else { '' }
            [pscustomobject]@{
                sha256 = $first.Sha256
                peMachine = '0x8664'
                winbindexSigningStatus = $first.WinbindexSigningStatus
                peTimestamp = ('0x{0:X8}' -f $first.Timestamp)
                imageSize = (ConvertTo-HexValue -Value $first.ImageSize)
                downloadUrl = if ($downloadKey) { "https://msdl.microsoft.com/download/symbols/controlcenter.dll/$downloadKey/controlcenter.dll" } else { '' }
                associations = @(
                    $group.Group |
                        Sort-Object WindowsFamily, @{ Expression = { [version]$_.ReleaseVersion } }, Update |
                        Select-Object WindowsFamily, ReleaseVersion, Update, UpdateDate, UpdateTitle, UpdateUrl,
                            ProcessorArchitecture, BuildType, PreviewTagged, FileVersion, BuildMatchEvidence
                )
            }
        }
    )

    $generatedAt = [DateTime]::UtcNow.ToString('O')
    $buildSlug = if ($Build) { $Build.Replace('.', '-') } else { 'all' }
    $inventoryPath = Join-Path $OutputDirectory "winbindex-controlcenter-$buildSlug.json"
    $inventory = [ordered]@{
        schemaVersion = 1
        generatedAtUtc = $generatedAt
        source = 'Winbindex x64 metadata; DLL downloads resolve to the Microsoft public symbol server.'
        sourcePage = $indexPage
        sourceData = $indexUri
        scope = 'AMD64 ControlCenter.dll candidates. Index metadata does not establish ABI compatibility or GA status.'
        requestedBuild = $Build
        candidateHashCount = $candidates.Count
        associations = $records.Count
        candidates = $candidates
    }
    $inventoryJson = ConvertTo-Json -InputObject $inventory -Depth 16
    Set-Content -LiteralPath $inventoryPath -Value $inventoryJson -Encoding utf8NoBOM

    $scanReports = [System.Collections.Generic.List[object]]::new()
    if ($Download -or $ScanAll) {
        foreach ($candidate in $candidates) {
            $hash = [string]$candidate.sha256
            $stage = 'metadata'
            try {
                if ([string]::IsNullOrWhiteSpace([string]$candidate.downloadUrl)) {
                    throw 'Missing symbol-server locator metadata.'
                }

                $downloadDirectory = Join-Path (Join-Path $OutputDirectory 'downloads') $hash
                New-Item -ItemType Directory -Path $downloadDirectory -Force | Out-Null
                $dllPath = Join-Path $downloadDirectory 'ControlCenter.dll'
                if (Test-Path -LiteralPath $dllPath -PathType Leaf) {
                    $stage = 'cached_hash_check'
                    $actualHash = (Get-FileHash -LiteralPath $dllPath -Algorithm SHA256).Hash.ToLowerInvariant()
                } else {
                    $stage = 'download'
                    $downloadBytes = Get-HttpBytes -Client $client -Uri $candidate.downloadUrl
                    $stage = 'download_hash_check'
                    $actualHash = [Convert]::ToHexString([System.Security.Cryptography.SHA256]::HashData($downloadBytes)).ToLowerInvariant()
                    if ($actualHash -eq $hash) {
                        [System.IO.File]::WriteAllBytes($dllPath, $downloadBytes)
                    }
                }
                if ($actualHash -ne $hash) {
                    throw 'Downloaded or cached bytes do not match the indexed SHA-256.'
                }

                $stage = 'authenticode'
                $signature = Get-AuthenticodeSignature -FilePath $dllPath
                $stage = 'compatibility_scan'
                $scan = Invoke-CompatibilityScanner -Path $dllPath -Scanner $ScannerPath
                $scanReport = [ordered]@{
                    schemaVersion = 1
                    generatedAtUtc = [DateTime]::UtcNow.ToString('O')
                    sourcePage = $indexPage
                    sourceDownload = $candidate.downloadUrl
                    build = $Build
                    sha256VerifiedAgainstIndex = $true
                    sha256 = $hash
                    winbindexSigningStatus = [string]$candidate.winbindexSigningStatus
                    authenticodeStatus = [string]$signature.Status
                    authenticodeCheckScope = 'scanner host signature lookup; source package catalog was not separately verified'
                    authenticodeSigner = if ($signature.SignerCertificate) { [string]$signature.SignerCertificate.Subject } else { '' }
                    compatibilityScanExitCode = $scan.ExitCode
                    compatibilityScan = $scan.Result
                    associations = $candidate.associations
                    reviewState = 'candidate-only; not added to the profile table and never loaded into Shell'
                }
                $reportPrefix = if ($ScanAll) { 'scan-all' } else { "scan-$buildSlug" }
                $scanReportPath = Join-Path $OutputDirectory "$reportPrefix-$hash.json"
                Set-Content -LiteralPath $scanReportPath -Value (ConvertTo-Json -InputObject $scanReport -Depth 16) -Encoding utf8NoBOM
                $scanReports.Add([pscustomobject]@{
                    Sha256 = $hash
                    Outcome = if ($scan.Result.supported) { 'profile_match' } elseif ($scan.Result.status -eq 'unknown_binary') { 'unknown_binary' } else { 'metadata_mismatch' }
                    Status = [string]$scan.Result.status
                    Supported = [bool]$scan.Result.supported
                    ProfileId = [string]$scan.Result.profileId
                    AuthenticodeStatus = [string]$signature.Status
                    WinbindexSigningStatus = [string]$candidate.winbindexSigningStatus
                    PreviewTaggedAssociations = @($candidate.associations | Where-Object PreviewTagged).Count
                    Report = [System.IO.Path]::GetFileName($scanReportPath)
                    FailureStage = ''
                    FailureType = ''
                })
            } catch {
                if (-not $ScanAll) { throw }
                $rootException = $_.Exception
                while ($null -ne $rootException.InnerException) { $rootException = $rootException.InnerException }
                $failureStatusCode = if ($rootException -is [System.Net.Http.HttpRequestException] -and $null -ne $rootException.StatusCode) {
                    [int]$rootException.StatusCode
                } else { 0 }
                $reportName = "scan-all-$hash.json"
                $failureReport = [ordered]@{
                    schemaVersion = 1
                    generatedAtUtc = [DateTime]::UtcNow.ToString('O')
                    sourcePage = $indexPage
                    sourceDownload = $candidate.downloadUrl
                    sha256 = $hash
                    sha256VerifiedAgainstIndex = $false
                    outcome = 'candidate_error'
                    failureStage = $stage
                    failureType = $rootException.GetType().FullName
                    failureStatusCode = $failureStatusCode
                    associations = $candidate.associations
                    reviewState = 'candidate-only; download or inspection failed; not added to the profile table and never loaded into Shell'
                }
                $failureReportPath = Join-Path $OutputDirectory $reportName
                Set-Content -LiteralPath $failureReportPath -Value (ConvertTo-Json -InputObject $failureReport -Depth 16) -Encoding utf8NoBOM
                $scanReports.Add([pscustomobject]@{
                    Sha256 = $hash
                    Outcome = 'candidate_error'
                    Status = 'candidate_error'
                    Supported = $false
                    ProfileId = ''
                    AuthenticodeStatus = ''
                    WinbindexSigningStatus = [string]$candidate.winbindexSigningStatus
                    PreviewTaggedAssociations = @($candidate.associations | Where-Object PreviewTagged).Count
                    Report = [System.IO.Path]::GetFileName($failureReportPath)
                    FailureStage = $stage
                    FailureType = $rootException.GetType().FullName
                    FailureStatusCode = $failureStatusCode
                })
            }
        }
    }

    if ($ScanAll) {
        $bulkSummaryPath = Join-Path $OutputDirectory 'scan-all-summary.json'
        $bulkSummary = [ordered]@{
            schemaVersion = 1
            generatedAtUtc = [DateTime]::UtcNow.ToString('O')
            sourcePage = $indexPage
            sourceData = $indexUri
            candidateHashCount = $candidates.Count
            associationCount = $records.Count
            profileMatchCount = @($scanReports | Where-Object Outcome -eq 'profile_match').Count
            unknownBinaryCount = @($scanReports | Where-Object Outcome -eq 'unknown_binary').Count
            metadataMismatchCount = @($scanReports | Where-Object Outcome -eq 'metadata_mismatch').Count
            candidateErrorCount = @($scanReports | Where-Object Outcome -eq 'candidate_error').Count
            authenticodeNotSignedStatusCount = @($scanReports | Where-Object AuthenticodeStatus -eq 'NotSigned').Count
            previewTaggedCandidateCount = @($scanReports | Where-Object PreviewTaggedAssociations -gt 0).Count
            reviewState = 'inventory and candidate scans only; no GA classification, profile-table change, or Shell load'
            results = @($scanReports)
        }
        Set-Content -LiteralPath $bulkSummaryPath -Value (ConvertTo-Json -InputObject $bulkSummary -Depth 16) -Encoding utf8NoBOM
        [pscustomobject]@{
            mode = 'full-index-scan'
            candidateHashCount = $candidates.Count
            profileMatchCount = $bulkSummary.profileMatchCount
            unknownBinaryCount = $bulkSummary.unknownBinaryCount
            metadataMismatchCount = $bulkSummary.metadataMismatchCount
            candidateErrorCount = $bulkSummary.candidateErrorCount
            authenticodeNotSignedStatusCount = $bulkSummary.authenticodeNotSignedStatusCount
            previewTaggedCandidateCount = $bulkSummary.previewTaggedCandidateCount
            inventoryReport = [System.IO.Path]::GetFileName($inventoryPath)
            scanReport = [System.IO.Path]::GetFileName($bulkSummaryPath)
            profileTableChanged = $false
            shellLoaded = $false
        } | ConvertTo-Json -Compress
    } elseif ($Download) {
        [pscustomobject]@{
            mode = 'download-and-scan'
            requestedBuild = $Build
            candidateHashCount = $candidates.Count
            inventoryReport = [System.IO.Path]::GetFileName($inventoryPath)
            scanResults = @($scanReports)
            profileTableChanged = $false
            shellLoaded = $false
        } | ConvertTo-Json -Depth 8 -Compress
    } else {
        [pscustomobject]@{
            mode = 'inventory'
            requestedBuild = $Build
            candidateHashCount = $candidates.Count
            associationCount = $records.Count
            inventoryReport = [System.IO.Path]::GetFileName($inventoryPath)
            previewTaggedAssociations = @($records | Where-Object PreviewTagged).Count
            profileTableChanged = $false
            shellLoaded = $false
        } | ConvertTo-Json -Compress
    }
} finally {
    $client.Dispose()
}
