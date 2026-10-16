# Windows 11 builds 26100.9168 / 26200.9168 beta compatibility evidence

This candidate adds one exact `ControlCenter.dll` hash associated by Winbindex with both Windows 11 24H2 build 26100.9168 and 25H2 build 26200.9168 in Microsoft's KB5121003 update. The update is listed on Microsoft's release-information page for the General Availability channel. The profile remains beta because neither target Shell host has been tested.

| Fingerprint | Value |
|---|---|
| SHA-256 | `E35DD0B8940BC97DA379C6B3E6A5C5F6D1D6CCBDEE2D062D660148C30FCF37AF` |
| Update association | KB5121003, builds 26100.9168 and 26200.9168, AMD64 |
| PE timestamp / image size | `0x381C21E7` / `0x3D3000` |
| Microsoft Authenticode | Valid; signer `CN=Microsoft Windows, O=Microsoft Corporation, L=Redmond, S=Washington, C=US` |
| PDB identity | `37209F4E-83C8-8201-165D-6C7E2201B671`, age 1 |
| `IQuickActionViewModel` IID | `CA79615D-BFBA-58DC-8D55-741963985C48` |
| Initialize vtable slot / target RVA | slot 6 / `0xAC690` |

The exact public PDB identity matches the DLL's CodeView record. Compared with the existing 26300.9711 public PDB, normalized public symbol-name sets match for `PaginatedToggleButton` (901 names), `IPaginatedToggleButton` (87), and `IQuickAction` (759). Both public PDBs expose no DIA-enumerable UDT layouts, so those symbol-name matches were treated as search evidence rather than an ABI proof.

An isolated probe loaded this candidate DLL in a separate process on host build 26300.9711. It activated `ControlCenter.QuickActionViewModel`, confirmed the expected IID via `GetIids`, succeeded at `QueryInterface`, and read vtable slot 6 as RVA `0xAC690`, matching the candidate PDB's `IQuickActionViewModel::Initialize` implementation. The probe did not call `Initialize`, modify a model, or connect to ShellHost. Its dependency environment was build 26300.9711, not either target build.

The profile is included in the beta build to enable exact-host tests on 26100.9168 or 26200.9168. Tile visibility, both theme transitions, native drag/reorder, position persistence, Shell recreation, sign-in recovery, and uninstall cleanup remain unverified on those hosts. [Microsoft Windows 11 release information](https://learn.microsoft.com/en-us/windows/release-health/windows11-release-information), [KB5121003](https://support.microsoft.com/en-us/servicing/os/windows-11/2026/08/kb5121003-windows-11-24h2-25h2-security-update), [Winbindex project](https://github.com/m417z/winbindex).
