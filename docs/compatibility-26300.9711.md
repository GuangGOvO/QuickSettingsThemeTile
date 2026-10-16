# Windows 11 build 26300.9711 compatibility evidence

This profile supports one exact x64 `ControlCenter.dll` build. It does not enable a general fallback for other Windows versions.

| Fingerprint | Value |
|---|---|
| SHA-256 | `0A9A84B0B7ED6110A410BD76150CE0DA04DC29B901838155AB7CE2D8BF2FFB51` |
| PE timestamp | `0x7162BCEC` |
| `SizeOfImage` | `0x3EA000` |
| PDB | `ControlCenter.pdb`, GUID `69549732-A6CF-1C43-FDF7-ED1FD2227E42`, age 1 |
| `IQuickActionViewModel::Initialize` implementation slot | RVA `0xAC420` (slot 6) |

The matching PDB's public symbols identify `QuickActionViewModel::Initialize` at RVA `0xAC44C` and the `IQuickActionViewModel::Initialize` wrapper at RVA `0xAC420`. The current PE vtable at RVA `0x323330` points to that wrapper in slot 6. The PDB also retains the setters used by the tile (`FriendlyName`, `ContainingGroupName`, `CustomTemplateKey`, `QuickActionTitle`, and `IsHandlerRequired`) and the QuickActionGroup collection getters.

An isolated helper process loaded the system DLL directly, activated `ControlCenter.QuickActionViewModel`, and called `GetIids`. It returned the same four interface IDs observed on the previously supported build, including `CA79615D-BFBA-58DC-8D55-741963985C48`. `QueryInterface` for that IID succeeded, and reading slot 6 from the returned interface pointer yielded RVA `0xAC420`. The helper did not call `Initialize`, modify a model, or connect to the running ShellHost.

The product build then passed `--status` with `supported:true`. Enabling it returned `enabled:true` and `nativeActive:true` without restarting ShellHost, and the user confirmed the tile appeared in Quick Settings. Click behavior, drag behavior, and position persistence have not yet been rechecked on this build.

The local 0.3.3 compatibility scanner independently identified the current host as Windows build 26300, UBR 9711 and selected `win11-x64-26300.9711`. Its SHA-256, PE metadata, and PDB GUID/Age matched this record. The 0.3.3 installer `--status` reported the same profile, `enabled:true`, and an active Shell session under the `0.3.3-native` runtime generation. The tile's click, drag, and placement behavior have not been rechecked against this compatibility build.
