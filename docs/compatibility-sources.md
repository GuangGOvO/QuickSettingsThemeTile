# Compatibility candidate discovery and scanning

This workflow collects candidate `ControlCenter.dll` files for review. It does not decide whether a Windows release is GA, prove ABI safety, change `src/compatibility.h`, install the product, or load a candidate into Shell.

## Public candidate index

`tools/compat-discover.ps1` reads Winbindex's compressed `ControlCenter.dll` metadata. Winbindex associates PE hashes with Windows update and assembly metadata, and links downloadable binaries to Microsoft's public symbol server. The script filters AMD64 `Microsoft-Windows-Shell-ControlCenter` entries, writes an ignored JSON inventory, and can download one exact build association or scan every unique indexed hash. Before scanning a downloaded file, it checks that the SHA-256 matches the index entry.

Run from the repository root in PowerShell 7:

```powershell
# List every AMD64 hash currently present in the index
.\tools\compat-discover.ps1

# Find the component associated with one exact OS build
.\tools\compat-discover.ps1 -Build 26200.9711

# Download one selected candidate from Microsoft's public symbol server,
# verify its indexed hash, record available signature status, and scan it
.\tools\compat-discover.ps1 `
    -Build 26200.9711 `
    -Download `
    -ScannerPath .\compat-check-033\compat-scan.exe

# Download/cache and scan every unique AMD64 hash in the current index
.\tools\compat-discover.ps1 `
    -ScanAll `
    -ScannerPath .\compat-check-033\compat-scan.exe
```

## Exact public-symbol inspection

For one DLL candidate, `compat-pdb-fetch.ps1` reads the embedded CodeView PDB basename and identity, fetches that exact PDB from Microsoft's public symbol server, and checks the PDB's GUID/age before emitting selected public symbols. The PDB inspector uses Visual Studio's DIA SDK without registering or installing a COM server. It is a research aid only; it never changes the product profile table or loads anything into Shell.

Build the optional inspector, then scan a candidate DLL:

```powershell
.\build.ps1 -Target compatibility -BuildName compat-check-033
.\build.ps1 -Target pdb-inspector -BuildName compat-pdb-tools
.\tools\compat-pdb-fetch.ps1 `
    -DllPath '.compat-index\downloads\<sha256>\controlcenter.dll' `
    -ScannerPath .\compat-check-033\compat-scan.exe `
    -InspectorPath .\compat-pdb-tools\compat-pdb-inspect.exe
```

The download and TSV evidence remain under ignored `.compat-index/`. The helper rejects a PDB whose embedded identity does not match the DLL's RSDS record. Its output is limited to public symbol names and available public-member metadata. Some Microsoft public PDBs omit user-defined type layouts: the tested 26200.9168 candidate PDB had no DIA-enumerable UDTs, so matching names are search clues only, not a vtable/layout compatibility proof. A zero-match result is recorded as an empty symbol selection while the identity check still succeeds.

To compare one candidate with a previously checked PDB, pass both embedded identities and focus on the interfaces used by the tile:

```powershell
.\tools\compat-pdb-compare.ps1 `
    -ReferencePdb '.compat-index\symbols\ControlCenter-26300.9711.pdb' `
    -ReferenceGuid '{69549732-A6CF-1C43-FDF7-ED1FD2227E42}' `
    -ReferenceAge 1 `
    -CandidatePdb '.compat-index\pdb-e35dd0b8940bc97da379c6b3e6a5c5f6d1d6ccbdee2d062d660148c30fcf37af.pdb' `
    -CandidateGuid '{37209F4E-83C8-8201-165D-6C7E2201B671}' `
    -CandidateAge 1
```

The 26200.9168 and 26300.9711 PDBs had identical normalized public symbol-name sets for `PaginatedToggleButton` (901 names), `IPaginatedToggleButton` (87), and `IQuickAction` (759). DIA exposed zero UDT layouts in either PDB. This equality is useful to prioritize a candidate, but cannot establish vtable slots, interface GUIDs, or the `Initialize` call contract.

Microsoft classifies build 26200.9168 as a General Availability Channel release and lists KB5121003 for Windows 11 24H2 and 25H2. Winbindex associates its shared update title with the unknown AMD64 DLL hash `E35DD0B8940BC97DA379C6B3E6A5C5F6D1D6CCBDEE2D062D660148C30FCF37AF`. The hash is still unsupported because matching public names do not prove ABI compatibility. [Microsoft Windows 11 release information](https://learn.microsoft.com/en-us/windows/release-health/windows11-release-information), [KB5121003](https://support.microsoft.com/en-us/servicing/os/windows-11/2026/08/kb5121003-windows-11-24h2-25h2-security-update).

The x64 26H1 channel needs separate handling: Microsoft's release page lists version 26H1 in the General Availability Channel, but its version note limits the release to new devices. The sampled August–October x64 Catalog packages (KB5121000, KB5124012, KB5129194, KB5127646) are classified Insider Pre-Release, while the corresponding ARM64 packages are listed as Windows 11 Security Updates. The KB5127646 Winbindex title lacks the word `Preview`, showing why `PreviewTagged=false` cannot classify the package channel. The 28000.3231 PDB identity (`64B89726-EF97-9105-CCAC-F39836D92891`, age 1) matches its DLL, but the normalized public symbol sets differ from the 26300.9711 baseline for `PaginatedToggleButton` (1025 vs 901), `IPaginatedToggleButton` (100 vs 87), and `IQuickAction` (714 vs 759); neither PDB exposes UDT layouts. The candidate remains unknown and is not loaded. Keep this classification discrepancy visible instead of inferring that every architecture-specific package follows the version-level label. [26H1 release status](https://learn.microsoft.com/en-us/windows/release-health/status-windows-11-26h1), [Update Catalog search for 26H1 x64 packages](https://www.catalog.update.microsoft.com/Search.aspx?q=Windows+11+26H1+for+x64).

PDB availability and symbol content vary by build. A matching PDB identity establishes which candidate symbols were read, not that the shell ABI matches another profile. Keep unknown DLL hashes rejected until interface identity, relevant call sites, and exact-build runtime behavior have been reviewed.

The selector matches the exact build in the update title, assembly version, or index release version. This matters for cumulative updates that list multiple OS builds under one release record. The report preserves which metadata field matched. `PreviewTagged` is a title-text hint only; it is not a Windows channel classifier. Check official release and package records before describing a candidate as GA.

The 2026-10-16 Winbindex snapshot had 38 unique AMD64 hashes and 84 update associations. The batch scan retrieved all 38 and verified each downloaded byte stream against its indexed SHA-256: 2 matched existing profiles, 36 were rejected as unknown binaries, and 0 downloads failed. This is candidate inventory, not completed compatibility coverage. Thirty-four hashes had at least one update title containing `Preview`; this title-text hint is not a release-channel classifier.

The snapshot includes the shared KB5127648 payload for builds 26100.9711, 26200.9711, and 26300.9711, plus older profile hashes associated with 26100/26200 .9457 and .9445 updates. Those matches show identical `ControlCenter.dll` bytes, but do not replace host tests on those releases. The dataset still lacks a complete archive of every Windows 11 servicing build, including 23H2-era and earlier builds. Re-run the script when researching a newer build because this third-party dataset changes over time.

Each report records Winbindex's `signingStatus` metadata and the signature status returned by the scanning host. A `NotSigned` result means no applicable signature was found by that host lookup; the exact update package's `.cat` file was not downloaded or independently verified. Windows system files can be validated through catalog signatures, and PowerShell may use a catalog signature when the relevant catalog is available. See Microsoft's [`Get-AuthenticodeSignature` documentation](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.security/get-authenticodeSignature?view=powershell-7.5) and [SignTool verification examples for system files](https://learn.microsoft.com/en-us/windows/win32/seccrypto/using-signtool-to-verify-a-file-signature). Do not treat `NotSigned` alone as proof that a Microsoft package is unsigned or invalid.

## Microsoft packages and local archives

For builds missing from Winbindex, use Microsoft's Update Catalog or a Microsoft Windows installation/update image to obtain the matching package. UUP Dump can help locate Windows Update files, but it is a third-party project and its output must be independently checked. Keep extraction and servicing confined to a disposable offline image or working directory; do not add packages to the running Windows installation for this scan.

The 23H2 x64 investigation is recorded in [compatibility-23h2.md](compatibility-23h2.md). It corrects KB5126653 to OS build 22621.7652, verifies the Microsoft-signed package and Microsoft's update file list, and records the limits of the 22H2.1 UUP baseline. A missing DLL from one cumulative-update file list is not treated as proof that it is absent from the full installed OS.

Once the package has been extracted and the exact `ControlCenter.dll` located, scan one file:

```powershell
.\tools\compat-discover.ps1 `
    -DllPath 'D:\offline-image\Windows\System32\ControlCenter.dll' `
    -ScannerPath .\compat-check-033\compat-scan.exe
```

Or scan every file named `ControlCenter.dll` under an extracted package tree:

```powershell
.\tools\compat-discover.ps1 `
    -DllDirectory 'D:\offline-packages\candidate-build' `
    -ScannerPath .\compat-check-033\compat-scan.exe
```

For local files, `-Build` is only a caller-supplied label and is explicitly recorded as unverified. Reports include file hash, version, Winbindex or host signature status where applicable, and scanner output, but not the local source path. Scanner exit code 2 (`unknown_binary`) is a valid candidate result; it means the exact DLL is not in the current profile table.

## Review boundary

An exact known hash identifies the same DLL bytes, but does not establish compatibility of every surrounding Windows component or release family. For an unknown hash, inspect the exact matching symbols and implementation, verify the required interfaces and call sites, then test the candidate in an isolated VM running that build. Only a separate reviewed change may add a profile. The discovery script never edits the profile table and never loads a DLL into Shell.

Temporary inventories, downloaded binaries, and scan reports go under `.compat-index/`, which is git-ignored. Do not publish those machine-specific artifacts. Public packages must continue to exclude Microsoft system DLLs and symbols.
