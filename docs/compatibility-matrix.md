# Windows 11 x64 compatibility matrix

The product only loads a profile when the system `ControlCenter.dll` hash and required PE/ABI metadata match one entry in `src/compatibility.h`. PDB identity is checked for profiles that declare it. A scanner result is diagnostic input, not approval to load an unknown build.

Profile IDs retain the build label from the original ABI validation. The runtime matcher selects by DLL hash and PE/PDB metadata, not by the host OS build number. A matching binary found in an update for another build confirms the same DLL fingerprint, but does not by itself confirm Shell behavior on that host.

## Current profiles

| Windows build | Profile | DLL SHA-256 | ABI evidence | Current UI evidence |
|---|---|---|---|---|
| 26300.9457 | `win11-x64-26300.9457` | `3E9A30BD1E92220BAEEBA90CD0AC409AE541C650019AD7C80AEC97DAF39C3261` | PE timestamp `0xF0B032A1`, image size `0x3D3000`, Initialize RVA `0xAC6A0`; profile does not require a PDB identity | Historical prototype interaction was checked; the 0.3.3 binary has not been installed on this build |
| 26300.9711 | `win11-x64-26300.9711` | `0A9A84B0B7ED6110A410BD76150CE0DA04DC29B901838155AB7CE2D8BF2FFB51` | PE timestamp `0x7162BCEC`, image size `0x3EA000`, required PDB GUID `69549732-A6CF-1C43-FDF7-ED1FD2227E42` age 1, Initialize RVA `0xAC420` | User confirmed the 0.3.2 tile appeared; current 0.3.3 status is enabled with an active Shell session, but click, drag, and order persistence have not been rechecked on this build |
| 26100.4946 | `win11-x64-26100.4946` (beta candidate) | `C1786EC32A32F3815A92F2895E60FDE938033BB24007AD6AF11856C9C985491E` | PE timestamp `0xDCBA0B0B`, image size `0x3B8000`, PDB GUID `EAFC8570-D54B-672B-7815-F76B54D84031` age 1; matching `IQuickActionViewModel` IID and slot 6, candidate Initialize RVA `0xACFB0` | Isolated activation/QI/vtable probe passed on host 26300.9711; 26100.4946 Shell UI test pending. Beta validation only |
| 26200.9168 / 26100.9168 | `win11-x64-26200.9168` (beta candidate) | `E35DD0B8940BC97DA379C6B3E6A5C5F6D1D6CCBDEE2D062D660148C30FCF37AF` | PE timestamp `0x381C21E7`, image size `0x3D3000`, PDB GUID `37209F4E-83C8-8201-165D-6C7E2201B671` age 1; matching `IQuickActionViewModel` IID and slot 6, Initialize RVA `0xAC690` | Isolated activation/QI/vtable probe and Microsoft Authenticode check passed on host 26300.9711; both target Shell UI tests pending. Beta validation only |

The 26300 profile hashes also appear in later Winbindex update associations for 24H2/25H2 builds, described below. The 26100.4946 profile is a beta candidate based on an exact user-reported installed-file hash; package catalog verification and target Shell UI acceptance remain pending. The 26100/26200.9168 profile maps one shared hash associated with the August 2026 KB5121003 update; its Microsoft Authenticode signature and isolated ABI contract passed, but target Shell UI acceptance remains pending. Other distinct DLL hashes remain unknown and fail closed. This is not complete Windows 11 coverage.

## Official release-family inventory

Snapshot date: 2026-10-16. The Windows release-information overview still showed September's servicing baseline when checked, so this inventory uses Microsoft's October 13 update notes and Update Catalog entries where available. The four exact DLL profiles comprise two original ABI baselines (26300.9457 and 26300.9711) and two beta candidates (26100.4946 and the shared 26100/26200.9168 component). Each distinct monthly/OOB DLL hash still needs review. [Windows 11 release information](https://learn.microsoft.com/en-us/windows/release-health/windows11-release-information), [October 13 KB5127648 notes](https://support.microsoft.com/en-gb/servicing/os/windows-11/2026/10/kb5127648-windows-11-24h2-25h2-26h2-security-update), and [Microsoft Update Catalog, October 2026](https://www.catalog.update.microsoft.com/Search.aspx?q=Windows+11+2026%2F10).

| Version | October 2026 x64 update evidence | DLL profile state |
|---|---|---|
| 26H2 | 26300.9711, KB5127648 | Existing exact profile; candidate downloaded, hash/signature checked, and scanner matched it |
| 26H1 | 28000.3231, KB5127646; Microsoft lists 26H1 in the GA channel for new devices | Unknown DLL hash; public symbol differences and absent UDT layouts leave ABI unproven, so it remains unsupported |
| 25H2 | 26200.9711, KB5127648 | Same DLL hash as the 26300.9711 profile; 26200.9457 and .9445 updates also map to the 26300.9457 profile hash. 26200.9168 uses the separate KB5121003 beta hash; no 26200 host test |
| 24H2 | 26100.9711, KB5127648 | Same DLL hash as the 26300.9711 profile; 26100.9457 and .9445 updates also map to the 26300.9457 profile hash. Historical 26100.4946 has a separate beta profile; 26100.9168 shares the KB5121003 beta hash with build 26200.9168. No 26100 host UI test |
| 23H2 | October x64 cumulative update KB5126653, OS build 22621.7652 | No AMD64 `ControlCenter.dll` candidate in Winbindex or the current update file list. The 22H2 base-image inventory points to the older Quick Actions files, but the exact current 23H2 image and host behavior remain unverified. No profile; see the [23H2 evidence record](compatibility-23h2.md) |
| 22H2 and earlier | Historical releases | Not yet inventoried |

Microsoft's KB5127648 notes confirm the October 13 builds 26300.9711, 26200.9711, and 26100.9711. The current release-information page lists 26H1 in the General Availability Channel and limits it to new devices rather than in-place feature updates. Treat that official release classification as GA; its unknown x64 DLL still requires ABI review. ARM64 remains outside this work.

Microsoft identifies KB5126653 as Windows 11 23H2 OS build 22621.7652. Do not label this release 22631 based on the version name; 22H2 and 23H2 share a common core and identical system-file set. The 23H2 evidence record distinguishes current-update inventory from a complete installed-image inventory.

## Winbindex batch scan snapshot

The 2026-10-16 index contained 38 unique AMD64 DLL hashes and 84 update associations. All 38 downloads matched their indexed SHA-256. The initial scan matched two profiles and rejected 36 candidates. Later isolated ABI reviews added the 26100.4946 and shared 26100/26200.9168 hashes as beta candidates, so the table now recognizes four of those hashes; the remaining 34 still fail closed. The current profile hashes have these additional update-title associations:

| Existing profile | Winbindex associations | Acceptance boundary |
|---|---|---|
| `win11-x64-26300.9711` (`0A9A84B0…2FFB51`) | KB5127648: 26100.9711, 26200.9711, 26300.9711; KB5124010: 26100/26200/26300.9550 Preview | Same DLL fingerprint; only the 26300 host has product runtime evidence |
| `win11-x64-26300.9457` (`3E9A30BD…F39C3261`) | KB5124008: 26100/26200.9445; KB5129195: 26100/26200.9457 Out-of-band | Same DLL fingerprint; 26100/26200 hosts are untested |
| `win11-x64-26100.4946` (`C1786EC3…85491E`) | KB5063878: 26100.4946 | Exact candidate ABI contract passed isolated activation, IID, QI, and slot checks on a 26300 host; target Shell integration pending |
| `win11-x64-26200.9168` (`E35DD0B8…FCF37AF`) | KB5121003: 26100/26200.9168 | Microsoft Authenticode is valid; exact candidate ABI contract passed isolated activation, IID, QI, and slot checks on a 26300 host; target Shell integration pending |

The scan host reported `Authenticode=Valid` for four candidates and `NotSigned` for 34. The corresponding update `.cat` files were not fetched or verified, so `NotSigned` is not treated as proof of an invalid Microsoft package. Thirty-four candidate hashes have at least one update title containing `Preview`; title text alone does not establish the release channel. The scan reports and binaries are local, ignored artifacts under `.compat-index/` and are not part of a release.

## First GA unknown-hash triage

Microsoft lists builds 26100.9168 and 26200.9168 in KB5121003, and the 25H2 release information classifies that release as General Availability. Winbindex associates their shared AMD64 component hash with `E35DD0B8940BC97DA379C6B3E6A5C5F6D1D6CCBDEE2D062D660148C30FCF37AF`. The downloaded DLL has a valid Microsoft Authenticode signature. Its exact PDB identity (`37209F4E-83C8-8201-165D-6C7E2201B671`, age 1) was verified. Compared with the existing 26300.9711 PDB, normalized public symbol names were identical for `PaginatedToggleButton` (901), `IPaginatedToggleButton` (87), and `IQuickAction` (759), while both public PDBs exposed zero DIA-enumerable UDT layouts. The isolated ABI probe independently activated `ControlCenter.QuickActionViewModel`, confirmed the required IID and `QueryInterface`, and read slot 6 as the PDB `Initialize` RVA `0xAC690`. This is a beta profile only; neither target host's Shell behavior has been tested. [Microsoft release information](https://learn.microsoft.com/en-us/windows/release-health/windows11-release-information), [KB5121003](https://support.microsoft.com/en-us/servicing/os/windows-11/2026/08/kb5121003-windows-11-24h2-25h2-security-update).

The latest indexed 26H1 x64 candidate is build 28000.3231 / KB5127646, DLL hash `1753C39539EF555055768664E5EF0B39881C0F1F41196705120C9C22DDD906BB`; it is still `unknown_binary`. Its exact PDB identity (`64B89726-EF97-9105-CCAC-F39836D92891`, age 1) was verified. Compared with the existing 26300.9711 PDB, normalized public symbol sets differ: `PaginatedToggleButton` 1025 vs 901 names, `IPaginatedToggleButton` 100 vs 87, and `IQuickAction` 714 vs 759. Neither PDB exposes DIA-enumerable UDT layouts. These differences reinforce that this hash cannot reuse the existing profile; no profile has been added. Microsoft lists 26H1 in the GA channel, while the Catalog calls the x64 KB5127646 package Insider Pre-Release, so its eligibility for the user's x64 GA target needs clarification. [Microsoft release information](https://learn.microsoft.com/en-us/windows/release-health/windows11-release-information), [Update Catalog search for KB5127646](https://www.catalog.update.microsoft.com/Search.aspx?q=KB5127646).

## Adding a profile

1. On the candidate Windows build, run the developer-only `compat-scan.exe` against the system DLL or a preserved `ControlCenter.dll`. Record the output privately; it contains no full path, SID, process ID, or UI tree.
2. Confirm the DLL is an authentic Microsoft system component and retrieve matching public symbols when available. Review the required QuickAction type, IID, setters, collection access, vtable slot, and Initialize implementation. A matching PDB alone does not establish ABI safety.
3. Add a profile entry only after the interface has been checked in an isolated process. Unknown builds must continue to fail closed while this work is pending.
4. Build and run the compatibility parser tests, then test the candidate on a VM or host running that exact build. Verify tile display, both theme transitions, native drag/reorder, position after reopening Quick Settings, Shell recreation, and sign-in recovery.
5. Publish the profile with a new product version and keep the previous exact profiles and a rollback package available.

The first release goal is Windows 11 x64 official builds. ARM64 and Insider Preview builds are out of scope for this matrix.
