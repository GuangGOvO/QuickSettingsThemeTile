# 公开版本验证记录

现场验证日期：2026-10-06（0.3.0）。公开包构建日期：2026-10-07（0.3.1）。兼容性构建日期：2026-10-16（0.3.2–0.3.6 Beta）。下表区分旧原型、现场安装版、公开测试包和仍待验收项目。错误 HRESULT 与处理逻辑保留，公开构建的 C++/WinRT 异常不携带源文件名和行号。

## 检查结果

| 检查 | 结果与范围 |
|---|---|
| 原生首页显示 | 旧结构原型得到人工确认 |
| 原生拖动与关闭后重开位置保留 | 旧结构原型得到人工确认；不等同于注销恢复 |
| 真实浅色/深色切换与同排对齐 | 旧功能 V3 得到人工确认，两组写入与读回 error=0 |
| 真正的原生磁贴控件 | 已接入 ControlCenter.PaginatedToggleButton，绑定原生命令和 IsChecked |
| 原生背景与边框 | V5 人工截图确认存在；复用邻近 Style、Template |
| 最终描边月亮尺寸、位置 | 16 DIP、向左 1 DIP 已构建加载；最终视觉判断未验收 |
| 安装版原生集合插入 | 原生日志确认默认/用户/显示集合分别为 23/23/12 |
| 启用→卸载→再次启用 | 0.3.0 安装生命周期通过 |
| 组件映射 | 确认宿主加载的是实际安装目录中的 DLL |
| 重复任务与会话去重 | 主动触发任务后结果为 0，没有新增诊断会话 |
| 安装器、加载程序退出 | 确认操作后均无进程，调试接入器也已停止 |
| 卸载撤销订阅与自建磁贴 | 控制器停止、模型移除、Unadvise 成功、SESSION FINISHED |
| 卸载配置、任务、位置文件 | 均已删除，活动会话为空 |
| Windows 11 26300.9711 DLL 指纹 | 当前微软签名的 ControlCenter.dll 哈希与统一档案表匹配；0.3.3 `--status` 识别为 `win11-x64-26300.9711` |
| 统一兼容档案 | 哈希、PE 元数据、DLL CodeView 要求和 Initialize RVA 共用 `src/compatibility.h` 中的单一档案表 |
| PE/CodeView 扫描器 | 当前系统 DLL 输出 build profile 26300.9711、SHA-256、PE 时间戳、映像大小和 DLL 内 CodeView GUID/Age |
| 公共候选索引 | 2026-10-16 Winbindex 快照：38 个 AMD64 哈希、84 条更新/组件关联；数据不证明 GA 状态或 ABI 安全 |
| 26H1 x64 渠道分类 | 微软版本页列 26H1 为面向新设备的 GA 版本；KB5127646 的 x64 Catalog 项标记 Insider Pre-Release，ARM64 项为常规安全更新；本项目的 x64 GA 档案不包含该 x64 Insider 包 |
| 26H1 x64 DLL 筛查 | 28000.3231 候选 PDB 身份核对及隔离激活/IID/slot 筛查通过；公开符号集合与 26300.9711 有差异，且 x64 包分类为 Insider；未加入产品档案 |
| 0.3.5 Beta 1 Winbindex 全候选扫描 | 38/38 DLL 下载哈希与索引匹配；当时扫描器首轮2个匹配、36个未知；19 个 GA 指纹加入后 23 个档案命中、15 个仍拒绝，0 个 DLL 下载失败 |
| 微软公共 PDB 候选审查 | GA 候选 26200.9168 的 DIA-reported GUID/Age 与 DLL CodeView key 一致，但 PDB Info-stream age 为 3；符号仅作线索，实际 slot/RVA 由隔离 ABI 探针核验后作为 Beta 档案加入 |
| 26100.4946 精确候选 | DLL SHA-256 与用户报告的系统 DLL 和 Winbindex 索引相同；PE/PDB 指纹、`IQuickActionViewModel` IID、vtable slot 6 / Initialize RVA 通过静态核验 |
| 26100.4946 隔离 ABI 探针 | 激活、GetIids、QueryInterface、slot 6 RVA 均通过；运行宿主为 build 26300.9711，未调用 Initialize 或连接 ShellHost，不能替代目标 build 测试 |
| 26100/26200.9168 精确候选 | KB5121003 关联的共享 DLL SHA-256、PE/CodeView 指纹通过；`Get-AuthenticodeSignature` 返回 Valid，签名者为 Microsoft Windows |
| 26100/26200.9168 隔离 ABI 探针 | 激活、GetIids、QueryInterface、slot 6 `0xAC690` 均通过；运行宿主为 build 26300.9711，未调用 Initialize 或连接 ShellHost |
| 0.3.4 Beta 兼容性检查 | 39 项 profile / PE 检查通过；12 项主题后端事务检查通过，未修改实际主题 |
| 0.3.5 Beta 候选 ABI 批次 | 24 个 26100/26200 未知哈希中，23 个通过隔离激活、唯一 IID、QueryInterface 和 Initialize 槽检查；19 个有 GA 更新关联而新增，4 个仅关联预发布候选而排除 |
| 0.3.5 Beta 1 历史候选边界 | 发布 Beta 1 时，26100.4202–4351 因候选 PDB Info-stream age 3 与 DLL CodeView age 1 不一致而保持 `unknown_binary`；0.3.6 仅在精确 DLL 通过直接 ABI 探针后补入此指纹 |
| 0.3.5 compatibility/profile tests | 260 项通过；覆盖 23 个精确 profile、唯一 PE metadata、哈希/PDB 偏差和损坏 PE 拒绝 |
| 0.3.6 Beta 1 的 26100.4202 档案 | 本地扫描器返回 `supported:true` 和 profile `win11-x64-26100.4202`；精确 DLL SHA-256 为 `1E7A7F673297B4A2023FEAD98DBEF8C5066A279EABCACA94DCCA16C44740DCDC` |
| 0.3.6 Beta 1 的 26100.4202 隔离 ABI 探针 | build 26300.9711 独立进程激活和 QueryInterface 成功，唯一 IID 匹配，slot 6 实际 RVA `0xACF90`；未调用 Initialize、未触及 Shell |
| 0.3.6 Beta 1 profile / PE parser tests | 275 项通过；覆盖 24 个档案及新增 26100.4202 的 SHA-256、PE 元数据、CodeView 身份、RVA 与 IID |
| 0.3.5 x64 product build | MSVC 产品构建成功；`ThemeTileSetup.exe` SHA-256 `F99682687667C32B33B7DA7615D6B3D8BD6295BF5A67776DE6FCD5333723EE5D` |
| 0.3.6 Beta 1 x64 product build | MSVC 安装器构建成功；`ThemeTileSetup.exe` SHA-256 `52697BFDEA4643D360492120B4DEC9106154F3B448AAF73AFDE194D32F7CFA9D` |
| 当前本地安装器 `--status` | 退出码 0；当前主机识别为 `win11-x64-26300.9711`，`enabled:true`、`supported:true`、`shellSessionActive:true`、`runtimeInstalled:false`；仅查询状态，未改变接入 |
| 0.3.6 Beta 1 安装器 `--status` | 退出码 0；识别当前主机为 `win11-x64-26300.9711`，`supported:true`、`shellSessionActive:true`、`runtimeInstalled:false`；只读检查，未重启 Shell |
| 0.3.5 installer `--status` smoke test | 只读返回 `supported:true`、`profileId=win11-x64-26300.9711`、`shellSessionActive:true`、`runtimeInstalled:false`；未安装 0.3.5 组件、未重启 Shell、未切断既有会话 |
| 0.3.6 目标 Shell UI | 新增的 26100.4202 档案尚未在对应 Windows build 手动测试；ABI 探针未连接 ShellHost。旧产品在 26300.9711 的已有用户验收不替代本 beta 回归 |
| 23H2/22H2 旧架构 | Winbindex 关联 QuickActionsDataModel.dll、Windows.UI.QuickActions.dll、CustomShellHost.exe；当前产品没有旧 Quick Actions provider 适配器，保持不支持 |
| 26100.4946 Beta Shell UI | 待用户在精确目标 build 安装验收 tile 显示、双向主题切换、拖动顺序、Shell 重建和卸载 |
| 26100/26200.9168 Beta Shell UI | 待相应 24H2 / 25H2 目标 build 安装验收；同一 DLL hash 仍不能替代不同 Shell 环境的运行检查 |
| 多 build 候选解析 | 26100/26200/26300.9711 映射到同一 `.9711` 档案哈希；26100/26200.9457 和 .9445 更新映射到 `.9457` 档案哈希；只是二进制关联，24H2/25H2 主机未验收 |
| 扫描时签名状态 | 扫描主机对 4 个候选返回 Authenticode `Valid`，34 个返回 `NotSigned`；未获取或验证更新包 `.cat`，因此不能据此判定包无效 |
| 离线包本地扫描入口 | `-DllPath` / `-DllDirectory` 已实现；其他候选 build 的离线提取包尚未实际扫描 |
| Profile / PE parser tests | 0.3.4 的 39 项仍记录于历史验证；0.3.5 为 260 项；0.3.6 为 275 项：24 个已知档案、新增哈希精确断言、未知哈希、PE/PDB 元数据偏差及有效/截断/损坏 PE |
| 未知 DLL 拒绝 | 扫描产品安装器返回 `unknown_binary`、`supported:false`，退出码 2；没有进入 Shell |
| 扩展后的 `--status` | 显示 `supported`、profileId、SHA-256、PE/PDB 元数据及布尔运行状态；不输出本地路径或进程编号 |
| OS build/UBR 识别 | `RtlGetVersion` 与当前版本 UBR 返回 26300 / 9711 |
| Windows 11 x64 GA 组件覆盖 | 0.3.6 Beta 1 有 24 个指纹，新增 26100.4202/.4349/.4351 共用组件；其目标 Shell UI 尚未验收。38 个 ControlCenter.dll 哈希中，10 个属于 26H1 x64 预发布包，4 个仅关联预发布候选而排除；23H2/22H2 旧 Quick Actions 架构仍需独立适配 |
| 当前 QuickAction IID 与初始化槽 | 独立进程激活对象；`QueryInterface` 成功，槽 6 为 `ControlCenter.dll+0xAC420`；未调用 `Initialize` |
| 26100.4946 真实 DLL 扫描 | 新兼容扫描器把本地缓存候选识别为 `win11-x64-26100.4946`、`supported:true`，SHA-256、PE 和 PDB 指纹全部匹配 |
| 26100.4946 隔离 ABI 探针 | 激活、IID、QueryInterface 和 slot 6 `0xACFB0` 均通过；探针运行在 26300.9711，不调用 Initialize、不接触 Shell |
| 26100.4946 产品安装及 Shell 行为 | 待目标机器人工验收；隔离探针通过不能替代快速设置磁贴、点击、拖动、Shell 恢复与卸载测试 |
| 0.3.2 产品启用 | 构建通过；`--enable` 返回 `enabled:true`、`nativeActive:true`，ShellHost 未重启 |
| 更新后快速设置磁贴重新显示 | 用户确认已出现 |
| 0.3.3 Beta `--status` | 新构建的安装器只读识别当前 DLL 为 `win11-x64-26300.9711`；该次检查显示 `runtimeInstalled:false`，没有安装 Beta、操作或重启 Shell，因此不构成安装验收 |
| 更新后点击、拖动与位置保持 | 尚未复验 |
| 卸载立即删除已映射 DLL | 无法立即删除；功能已停止，需正常宿主退出后再次清理文件 |
| 原有主题与其他磁贴排列 | 安装生命周期前后保留 |
| C++ 运行库依赖 | 产品 /MT，依赖检查没有 VCRUNTIME/MSVCP |
| 后端故障检查 | 12 项隔离事务检查通过，不修改真实主题 |
| 0.3.1 公开源码与安装器构建 | 构建通过，12 项后端检查再次通过；相对路径构建产物不含个人目录 |
| 0.3.1 公开包安装和升级 | 未测试；0.3.0 的现场结果不能替代本项 |
| 最终安装版实际点击与拖动不误触 | 未完成人工验收 |
| 登录触发器 | 当前用户、普通权限、交互令牌配置已检查；未实际注销登录 |
| 安装版宿主重建、系统重启恢复 | 未完成实际验收 |
| 跨页、键盘、触摸、高对比度、其他缩放 | 未完成验收 |

现场结果的摘要见 [lifecycle-summary.json](lifecycle-summary.json)。公开分发不包含原始控件树、截图、机器路径、用户 SID、进程编号或完整任务 XML。

## 实现与已发现的问题

内建 Microsoft.QuickAction.DarkTheme 在测试构建上不适用，修改原生布局仍会被过滤。当前方案创建自有模型，并加入默认、用户、显示三个向量，继续使用原生容器与拖动。

首次改为真正的原生按钮时，缺少 Style/Template，出现背景缺失和图标偏移。此后复制邻近原生 Style/Template，并将 E708 月亮从 20 改为 16 DIP，向左微调 1 DIP。最终视觉与安装版交互仍待验收。

早期安装器与 Shell 组件看到不同的注册表/文件状态，诊断初始化返回成功也没有活动会话。0.3.0 改为 Shell 包 LocalState 自有目录中的启用和位置文件，并从组件安装位置确定目录；后续现场加载与生命周期通过。不同视图可能与打包进程的数据重定向有关，这是工程推断。[微软数据虚拟化说明](https://learn.microsoft.com/en-us/windows/msix/desktop/flexible-virtualization)

较早诊断曾读取隐藏、未初始化模型的后端属性，导致 ControlCenter 访问异常。后续版本只读取缓存的名称、模板键、分组字段，不读取该类后端 Id。不能把已修正的单次诊断事故或较短观察时间当作长期稳定性证明。

## 适配与通知限制

公开索引与离线候选扫描命令、字段解释和适配边界见 [候选发现与扫描流程](compatibility-sources.md)。Winbindex 当前数据可把 26100/26200/26300 的 9711 更新关联到同一二进制；这一发现不会自动增加 build 兼容声明。缺少索引的历史版本需要从官方更新包或镜像中提取准确组件，再使用本地扫描入口。

0.3.2 仅接受以下两种已验证的 Windows 11 x64 系统组件：

```text
26300.9457  3E9A30BD1E92220BAEEBA90CD0AC409AE541C650019AD7C80AEC97DAF39C3261
26300.9711  0A9A84B0B7ED6110A410BD76150CE0DA04DC29B901838155AB7CE2D8BF2FFB51
```

26300.9457 检查 PE 时间戳 `0xf0b032a1`、映像大小 `0x3d3000` 和初始化槽 `0xAC6A0`。26300.9711 检查 PE 时间戳 `0x7162BCEC`、映像大小 `0x3EA000` 和初始化槽 `0xAC420`。当前构建的 PDB 标识、隔离进程 IID 查询和槽位读数见 [compatibility-26300.9711.md](compatibility-26300.9711.md)。其他二进制指纹仍拒绝接入。

历史 0.3.0 的系统组件 SHA-256：

```text
3E9A30BD1E92220BAEEBA90CD0AC409AE541C650019AD7C80AEC97DAF39C3261
```

主题后端仅修改当前用户 SystemUsesLightTheme 和 AppsUseLightTheme，具有失败回滚与并发修改检查。此前现场通知日志出现部分窗口未响应，不能保证所有第三方应用即时重绘。

计划任务每分钟短暂运行加载程序，登录时也触发；已有会话时立即退出。没有自建常驻服务。运行上限和重复规则参考 [微软计划任务文档](https://learn.microsoft.com/en-us/windows/win32/taskschd/repeating-a-task)。
