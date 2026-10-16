# Windows 11 build 26100.4946 beta compatibility evidence

This is a beta profile for one exact x64 `ControlCenter.dll` hash. It enables a manual integration test on the matching 26100.4946 host; it is not a general 24H2 fallback.

| Fingerprint | Value |
|---|---|
| SHA-256 | `C1786EC32A32F3815A92F2895E60FDE938033BB24007AD6AF11856C9C985491E` |
| Winbindex association | Windows 11 24H2 build 26100.4946, KB5063878, AMD64 |
| PE timestamp / image size | `0xDCBA0B0B` / `0x3B8000` |
| PDB identity | `EAFC8570-D54B-672B-7815-F76B54D84031`, age 1 |
| `IQuickActionViewModel` IID | `CA79615D-BFBA-58DC-8D55-741963985C48` |
| Initialize vtable slot / target RVA | slot 6 / `0xACFB0` |

The downloaded DLL hash matches the Winbindex index entry and the hash reported by a user's installed 26100.4946 system DLL. The matching public PDB identity was checked. Winbindex reports the DLL as unsigned; the KB5063878 update catalog file was not separately downloaded and verified, so package-catalog provenance remains an explicit limitation for this beta candidate.

The candidate PDB maps the `ControlCenter.IQuickActionViewModel` GUID to the IID above and identifies the implementation vtable at RVA `0x2F6E90`. Reading the exact candidate image shows slot 6 points to RVA `0xACFB0`, the candidate PDB's `IQuickActionViewModel::Initialize` implementation. The existing 26300.9711 profile uses the same IID and slot contract, with slot 6 pointing to `0xAC420`.

`compat-abi-probe.exe` was run in a separate process against both DLLs on host build 26300.9711. For the candidate it activated `ControlCenter.QuickActionViewModel`, found the expected IID, succeeded at `QueryInterface`, and read slot 6 as `ControlCenter.dll+0xACFB0`. It did not call `Initialize`, modify a model, or connect to ShellHost. Because this probe ran on 26300.9711, its dependency environment was not the target 26100.4946 environment.

Exact-filter public-symbol comparison against the 26300.9711 PDB found identical name sets for `Windows::Internal::QuickActions::IQuickAction` (109/109), `ControlCenter::IPaginatedToggleButton` (41/41), and the required `ControlCenter::IQuickActionViewModel` functions apart from one compiler-generated exception-handler label (`catch$2` versus `catch$4`). `IQuickActionGroup` has four candidate-only names and no reference-only names. Neither public PDB exposes UDT layouts. The concrete `PaginatedToggleButton` implementation also has public-symbol differences, so visual and interaction behavior must be tested on the exact target build.

The profile is included in the current expansion build only to allow that exact-host beta test. The 26100.4946 Shell UI, theme changes, drag/reorder persistence, Shell recreation, sign-in recovery, and uninstall cleanup have not yet been verified.
