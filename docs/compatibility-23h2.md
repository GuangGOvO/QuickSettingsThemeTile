# Windows 11 23H2 x64 compatibility evidence

Snapshot date: 2026-10-16. This record investigates whether the existing `ControlCenter.dll` adapter has a candidate for Windows 11 23H2 x64. It does not add or approve a runtime profile.

## Release identity

Microsoft's October 13, 2026 support record identifies KB5126653 as Windows 11 23H2 OS build **22621.7652** and applies it to all 23H2 editions. Microsoft also states that 22H2 and 23H2 share a common core operating system and an identical set of system files; 23H2 is enabled with KB5027397. These records correct the earlier 22631 build label. [KB5126653](https://support.microsoft.com/en-gb/servicing/os/windows-11/2026/10/kb5126653-windows-11-23h2-security-update), [KB5027397](https://support.microsoft.com/en-gb/servicing/os/windows-11/2023/05/kb5027397-feature-update-to-windows-11-version-23h2-by-using-an-enablement-package).

## Evidence collected

- The x64 KB5126653 MSU from Microsoft Update Catalog is 1,222,962,623 bytes, SHA-256 `98F3FCE8D9ECA7DD90B5A6F86BAA1DBAE0A44E5E2881266BD895104713DCC84B`, and has a valid Microsoft Corporation Authenticode signature.
- The update's `express.psf.cix.xml` index and Microsoft's [KB5126653 file-information CSV](https://download.microsoft.com/download/16ba8bf1-c975-4501-9df6-a5089719ef46/5126653.csv) contain no `ControlCenter.dll` entry. The CSV has 102,617 rows and SHA-256 `912B27AF501A793575A1C9D72C19849EAD50FED4C8D60ADD5775DEA901256021`. These are update-payload inventories, not complete inventories of every unchanged file already installed on a device.
- `tools/compat-discover.ps1 -Build 22621.7652` returns no AMD64 `ControlCenter.dll` association from the current Winbindex dataset. This is a missing public candidate, not proof that a particular host lacks the DLL.
- As a historical baseline, the UUP Dump API's [stable 22H2 22621.1 record](https://api.uupdump.net/get.php?id=356c1621-04e7-4e66-8928-03a687c3db73&lang=en-us&edition=professional&noLinks=1) identified a Professional image ESD with SHA-256 `29CED52B2B659D5BA353108D9EC4B243B3B5D99529058C38E97CAAFF051EC041`. A local 7-Zip archive listing found `QuickActionsDataModel.dll` and `Windows.UI.QuickActions.dll` paths, but no `ControlCenter.dll` path. UUP Dump is a third-party index; this baseline listing is supporting evidence only. Extracting the ESD's individual DLL payload failed, and DISM reported that image inspection requires elevation, which was not requested or attempted.

## Compatibility decision

There is no 23H2 DLL hash or matching PDB to compare with the supported profiles. The available evidence points to the earlier Quick Actions implementation, while the current 23H2 image has not been fully inventoried or exercised on a 23H2 host. Therefore 23H2 remains **unsupported and unverified** by this `ControlCenter.dll`-based adapter. No profile was added, no candidate was loaded into Shell, and the active host was not changed.

To close this gap, inspect a complete 23H2 x64 image or disposable 22621.7652 VM, identify the actual Quick Settings host/module, and then assess whether it needs a separate legacy adapter. Do not reuse a newer profile based only on the shared OS version or the fact that 22H2 and 23H2 share system files.
