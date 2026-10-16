# Windows 11 x64 兼容矩阵

快设置磁贴依赖 Windows Shell 内部 COM/XAML 接口。运行时只按 `ControlCenter.dll` 的 SHA-256、PE 指纹和 DLL 内 CodeView GUID/Age 识别；主机 build 号或相邻版本通过均不能代替精确二进制匹配。

本矩阵以 2026-10-16 的 Windows Release Information、Microsoft Update Catalog、Winbindex 快照和本地隔离 ABI 检查为依据。ABI 筛查只表示候选 DLL 在独立进程中激活成功、唯一接口 IID 的 vtable slot 6 指向匹配 PDB 的 `Initialize` 函数；探针没有调用该函数、没有连接 ShellHost。它不是目标系统的完整运行时验收。

## 当前覆盖

| Windows 版本 | 当前官方 x64 build | 本档案状态 | 主要边界 |
|---|---:|---|---|
| 26H2 | 26300.9711 | 识别 26300.9457 与 .9711 两个指纹；.9550 更新映射到 .9711 指纹 | 用户此前在 26300.9711 验证过磁贴交互；本 beta 更新尚未重新实测 |
| 25H2 | 26200.9711 | 多个 26200 月度组件档案，并复用与 26H2 相同的 .9457/.9711 文件指纹 | 26200.9168、其他新增构建的 Shell 行为均待目标机验收 |
| 24H2 | 26100.9711 | 覆盖已审查的月度组件档案；26100.4202/.4349/.4351 共用新增精确指纹 | 新增候选通过隔离 ABI 探针；该更新区间的目标 Shell UI 验收待补 |
| 26H1 | 28000.3231 | 不纳入 x64 GA 档案 | 版本页列为面向新设备的 GA 版本，但 KB5127646 的 x64 Catalog 包被分类为 Insider Pre-Release；ARM64 包的分类不同 |
| 23H2 | 22631.7652 | 不支持 | 使用旧 Quick Actions 数据模型/宿主；需独立适配器。Home/Pro 已结束更新，Enterprise/Education 更新支持截至 2026-11-10 |
| 22H2 及更早 | 旧版 | 不支持/未完整盘点 | 使用旧 Quick Actions 路径；22H2 所有版本均已结束更新 |

微软 release page 当前将 26H2、25H2、24H2 的最新 GA build 列为 26300.9711、26200.9711 和 26100.9711，并列出 23H2 的 22631.7652。26H1 被限定为新设备版本；x64 的 KB5127646 Catalog 项仍标为 Insider Pre-Release，所以本项目按“Windows x64 正式包”范围排除它。23H2 Home/Pro 已于 2025-11-11 结束更新，Enterprise/Education 截止 2026-11-10；22H2 Home/Pro 于 2024-10-08 结束，Enterprise/Education/IoT Enterprise 于 2025-10-14 结束。[Windows 11 release information](https://learn.microsoft.com/en-us/windows/release-health/windows11-release-information)，[KB5127646 Update Catalog 搜索](https://www.catalog.update.microsoft.com/Search.aspx?q=KB5127646)，[Home/Pro 生命周期](https://learn.microsoft.com/en-us/lifecycle/products/windows-11-home-and-pro)，[Enterprise/Education 生命周期](https://learn.microsoft.com/en-us/lifecycle/products/windows-11-enterprise-and-education-version-21h2)。

## 已识别的 24 个精确指纹

下表短哈希只用于阅读；完整 SHA-256、PE 指纹、DLL CodeView GUID/Age、接口 IID 和 Initialize RVA 均固定在 [`src/compatibility.h`](../src/compatibility.h)。同一二进制可能被多个 release/build 复用，profile ID 是发现时的代表 build，不是运行时版本判断。

| Profile | SHA-256 前 12 位 | 来源/关联 |
|---|---|---|
| `win11-x64-26300.9457` | `3E9A30BD1E92` | 26H2 基线；也关联 26100/26200.9445、.9457 |
| `win11-x64-26300.9711` | `0A9A84B0B7ED` | 26H2 .9550/.9711；也关联 24H2/25H2 .9711 |
| `win11-x64-26100.1742` | `EEFA8B94AA00` | 24H2 GA 基线组件 |
| `win11-x64-26100.2033` | `077FA4E4B2AB` | 24H2 October 2024 GA update |
| `win11-x64-26100.2161` | `9D5074F1B2C4` | 24H2 October 2024 OOB update |
| `win11-x64-26100.2454` | `F33F0C2E199C` | 24H2 November 2024 update family |
| `win11-x64-26100.3037` | `37B06332DEA9` | 24H2 January 2025 update |
| `win11-x64-26100.3323` | `959AF64A0BA1` | 24H2 February 2025 update |
| `win11-x64-26100.3624` | `D6850CFF93FF` | 24H2 March 2025 update |
| `win11-x64-26100.3915` | `ACD252B8D62F` | 24H2 April 2025 update |
| `win11-x64-26100.4202` | `1E7A7F673297` | 24H2 May/June 2025 shared component; associated with GA KB5060842 and KB5063060, and preview KB5058499 |
| `win11-x64-26100.4484` | `C961FD54FF12` | 24H2 June 2025 update |
| `win11-x64-26100.4946` | `C1786EC32A32` | 24H2 July/August 2025 candidate; Shell UI test pending |
| `win11-x64-26100.5074` | `F1684659B6E4` | 24H2 August 2025 update family |
| `win11-x64-26200.6899` | `27DEBB0B4765` | 25H2 October 2025; same component also associated with 26100.6725 |
| `win11-x64-26200.7019` | `AE5903681072` | 25H2 October 2025 OOB/update family |
| `win11-x64-26200.7309` | `5A832B5B673F` | 25H2 November 2025 update family |
| `win11-x64-26200.7705` | `46E26CF66CB8` | 25H2 January 2026 update family |
| `win11-x64-26200.7922` | `2645119925A6` | 25H2 February 2026 update family |
| `win11-x64-26200.8116` | `B6E6BC510A2D` | 25H2 March 2026 update family |
| `win11-x64-26200.8328` | `AABE326BD7F8` | 25H2 April 2026 update family |
| `win11-x64-26200.8524` | `998CDBED64F6` | 25H2 May/June 2026 update family |
| `win11-x64-26200.8737` | `4FCF9EE69AA2` | 25H2 June/July 2026 update family |
| `win11-x64-26200.9168` | `E35DD0B8940B` | KB5121003 shared component; also associated with 26100.9168 |

All 20 expanded fingerprints require their own exact PE CodeView identity and profile-specific IID. The older interface IID `{F8FE70C8-8A70-5E6B-ACE2-4E7A26D921B7}` appears in earlier 26100 profiles; later candidates use `{CA79615D-BFBA-58DC-8D55-741963985C48}`. The product does not infer an IID from OS build at runtime. A PDB symbol's Initialize RVA is admitted only after an isolated probe confirms that the candidate DLL's slot 6 points to the same RVA; the 26100.4202 PDB's Info-stream age is inconsistent with the executable's CodeView age, so its RVA is specifically backed by that direct binary check rather than a claim of exact PDB matching.

## Batch review and exclusions

The 2026-10-16 Winbindex snapshot contains 38 unique AMD64 `ControlCenter.dll` hashes and 84 update associations. The newly added `26100.4202` fingerprint is SHA-256 `1E7A7F673297B4A2023FEAD98DBEF8C5066A279EABCACA94DCCA16C44740DCDC`; its Winbindex record associates the same file with GA updates KB5060842 and KB5063060, plus preview KB5058499. The Microsoft symbol-server PDB has matching GUID but PDB Info-stream age 3 while the DLL CodeView record requires age 1; DIA alone had reported age 1 from the DBI stream. Microsoft `pdb-rs` directs readers to use the PDB Info-stream age when these streams disagree, and LLVM documents that executable matching uses the same GUID/Age pair from the PDB Info stream and PE CodeView record ([pdb-rs](https://github.com/microsoft/pdb-rs/blob/main/docs/pdb/dbi_stream.md), [LLVM PDB matching](https://llvm.org/docs/PDB/PdbStream.html#matching-a-pdb-to-its-executable)). We do not treat that PDB as an exact match. Instead, the exact candidate DLL passed an isolated ABI probe: COM activation and interface query succeeded, its unique slot-6 target matched RVA `0xACF90`, and the probe did not call `Initialize` or touch Shell. The RVA is therefore backed by direct candidate-binary evidence. This screening result does not replace target-build Quick Settings UI acceptance. Four hashes tied only to preview/pre-GA associations remain excluded.

The ten 28000 candidates belong to the 26H1 x64 package stream, which the Update Catalog currently classifies as Insider Pre-Release; they are outside this x64 GA release target. An ABI screening result alone does not change that classification.

## 23H2 and the legacy adapter gap

The October 2026 23H2 update is KB5126653. Its Winbindex file metadata associates the update with legacy modules such as `QuickActionsDataModel.dll`, `Windows.UI.QuickActions.dll`, and `CustomShellHost.exe`, while the current product adapter is built around `ControlCenter.dll` and `IQuickActionViewModel`. A 22621.1 baseline sample was extracted from a cached Microsoft-format UUP image package: the x64 `QuickActionsDataModel.dll` SHA-256 is `488A3C2B39A8A5403634196D512AB9B5640F94FFDE9BF270CB0D37C10C837C30` (405,504 bytes), with PE CodeView key `{9B3E135C-D340-4236-59DF-CBD5B35A085D}` age 1. The symbol-server PDB returned for that key exposes legacy symbols including `QuickActionsProvider::Initialize`, `get_AvailableQuickActions`, `QuickSettingDefinition::MakeQuickAction`, and `ControlCenterSettings::ReadQuickActionsList`; its PDB Info-stream age is 2, so it is not an exact PDB identity match. The symbols confirm a distinct provider/data-model path in the 22621.1 baseline only; they do not establish the 23H2 servicing-build ABI or a safe insertion contract. A separate exact-package review and a 23H2 Shell test are required before any legacy adapter can be shipped. [KB5126653](https://support.microsoft.com/en-us/servicing/os/windows-11/2026/10/kb5126653-windows-11-23h2-security-update), [Winbindex project](https://github.com/m417z/winbindex), [22621.1 UUP Dump record](https://api.uupdump.net/get.php?id=356c1621-04e7-4e66-8928-03a687c3db73&lang=en-us&edition=professional&noLinks=1).

## Acceptance boundaries

- Runtime configuration contains only the 24 reviewed profile hashes in `src/compatibility.h`; all other hashes remain fail-closed.
- `abi_contract_match` does not call `Initialize`, touch theme settings, or load a candidate into `ShellHost.exe`.
- Only Windows 11 x64 GA update fingerprints are considered here. 26H1 x64 Insider packages, ARM64, 23H2/22H2 legacy modules, and other preview builds are not supported by this beta.
- Exact target-build UI acceptance still requires checking visibility, both theme transitions, alignment, native drag/reorder, saved position after closing/reopening, Shell recreation, sign-in recovery, and uninstall cleanup.
- The build IDs in profile names are evidence labels. The runtime checks the actual DLL fingerprint and does not grant support merely because Windows reports a matching OS build.
