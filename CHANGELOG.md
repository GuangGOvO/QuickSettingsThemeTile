# Changelog

## 0.3.3-native Beta 1 — 2026-10-16（公开手动验收）

- 将已验证的 DLL 哈希、PE 指纹、可用 PDB 标识和 Initialize RVA 合并为单一档案表，启用、状态查询、加载器和 Shell 内部检查共用该表。
- 新增开发用 `compat-scan.exe`，输出 ControlCenter.dll 的机器类型、SHA-256、PE 时间戳、映像大小及 RSDS PDB GUID/Age；未知文件只生成识别结果，不会被自动加入支持表。
- `--status` 增加 Windows build/UBR、兼容状态、档案编号及去路径化的二进制指纹；设置界面从档案表显示已验证构建。
- `--status` 用布尔值报告组件文件和 Shell 会话状态，不输出本地安装路径或进程编号。
- 新增 PowerShell 候选检查流程：查询 Winbindex、按更新标题解析共享 build、批量下载并校验索引 SHA-256，或扫描离线更新包提取的 DLL；不修改档案表、不加载 Shell。
- 新增可选 DIA PDB 检查工具：按 DLL 的 CodeView PDB 名称、GUID 和 Age 从微软公共符号服务器取回对应 PDB，核对内部身份并输出筛选后的公开符号；不会将公开符号名误判为 ABI 布局证据。
- 2026-10-16 的 38 个候选哈希全部成功取回：2 个匹配现有档案，36 个仍未知；扫描主机的 `NotSigned` 状态不代替原更新包 `.cat` 验证。
- Windows 11 x64 GA build 26200.9168 的候选符号已核对身份；`PaginatedToggleButton`、`IPaginatedToggleButton`、`IQuickAction` 的公开名称集合与现有档案相同，但公开 PDB 没有可枚举的 UDT 布局，故未新增兼容档案。
- 修正 26H1 范围描述：微软版本页列其为 GA，但 KB5127646 的 x64 Catalog 项为 Insider Pre-Release；将 x64 分发状态标为未决，不再把整版 26H1 直接排除或自动纳入。
- 核对 28000.3231 候选 PDB 身份并与 26300.9711 比较；`PaginatedToggleButton`、`IPaginatedToggleButton` 与 `IQuickAction` 公开符号集合均有差异，继续拒绝该 DLL。
- 17 项档案选择和 PE 解析检查通过。当前仍仅有 26300.9457 与 26300.9711 两个档案；完整 Windows 11 x64 正式版构建覆盖尚未完成。

## 0.3.2-native — 2026-10-16（兼容性修复）

- 为 Windows 11 x64 build 26300.9711 增加精确的 `ControlCenter.dll` SHA-256 白名单和 `Initialize` ABI 指纹；原有 26300.9457 白名单继续保留。
- 使用独立进程验证当前 QuickAction 模型仍实现原 IID，`QueryInterface` 成功，接口第 6 槽指向 `ControlCenter.dll+0xAC420`；诊断未调用初始化方法。
- 新安装器启用后返回 `nativeActive:true`，已接入当前 ShellHost，未重启宿主；用户已确认磁贴重新显示，点击、拖动和位置保持仍待本次更新后复验。其他系统 DLL 指纹仍拒绝加载。

## 0.3.1-native — 2026-10-07（公开预发布）

- 使用相对源码和资源路径构建，并关闭 C++/WinRT 的源码位置诊断，避免在二进制中嵌入开发者个人目录。
- 更新产品版本、组件目录和产品 CLSID；主题与磁贴逻辑沿用 0.3.0。
- 产品构建和 12 项后端检查通过，公开包尚未实际安装；现场安装证据明确保留为 0.3.0。

## 0.3.0-native — 2026-10-06（本地验证）

- 在 Windows 11 原生快速设置集合中加入“深色模式”磁贴。
- 使用原生磁贴控件、Style、Template 和描边月亮。
- 同时切换系统与应用主题，保留原生拖动。
- 提供单文件启用/卸载工具，通过当前用户计划任务短时加载，无自建常驻服务。
- 使用共享文件保存启用与位置，修复早期安装器和 Shell 数据视图不一致的问题。
- 只适配 26300.9457 对应的 ControlCenter.dll；最终交互和登录/重启恢复仍未完成验收。
