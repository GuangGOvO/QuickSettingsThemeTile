# QuickSettingsThemeTile

在 Windows 11 原生快速设置中添加“深色模式”磁贴。沿用系统拖动、分页和磁贴模板，点击同时切换系统与应用的浅色/深色主题。

**0.3.3-native Beta 1 是供手动验收的预发布版：将 DLL 哈希、PE 元数据、PDB 标识与 ABI 入口统一到一张档案表，新增开发用 `compat-scan.exe`，并扩展 `--status` 的兼容诊断字段。它只包含两个已验证档案（26300.9457、26300.9711），不代表已适配所有 Windows 11 x64 正式版构建。本次通过了本地构建、17 项兼容性检查和 12 项后端事务检查；Beta 安装器尚未安装验收，请手动复验磁贴显示、双向切换、拖动和位置保持。**

## 下载与使用

从 [Releases](https://github.com/GuangGOvO/QuickSettingsThemeTile/releases) 下载对应版本的 `ThemeTileSetup.exe`。GitHub 会为每个版本 tag 自动提供对应的源代码 ZIP；旧版本也可能有额外的 Windows ZIP。

稳定版仍是 0.3.1-native；当前 Beta 1 首次加入 build 26300.9711 的兼容档案，并等待手动复验。未列入档案的系统会被拒绝接入。

当前预发布工具未进行代码签名，下载附件可用同页的 `SHA256SUMS.txt` 核对。

1. 双击 `ThemeTileSetup.exe`，选择“启用功能”。无需管理员权限。
2. 操作完成后关闭工具，打开任务栏右下角的快速设置。
3. 点击“深色模式”切换主题，拖动磁贴调整位置。
4. 删除功能时再次打开工具，选择“卸载功能”。当前主题和其他磁贴排列会保留。

工具使用原生 Windows 对话框，只提供启用、卸载和取消。启用后可以关闭、移动或删除安装工具；日常使用不依赖该工具运行。

从现场测试版 0.3.0 升级时，先用旧版工具卸载功能，再用新版启用。新版使用独立组件目录和产品 CLSID。旧版已映射的文件仍需宿主正常退出后用旧版工具清理；新版卸载器只清理自己的组件目录。

## 运行方式

该功能需要 DLL 组件，不能仅靠修改原生布局或注册表恢复。安装器为当前用户配置 Windows 计划任务，在登录时以及每分钟短暂运行加载程序。加载完成或发现已有会话后，加载程序退出；没有自建常驻服务或独立监测进程。

磁贴逻辑在 `ShellHost.exe` 内持续运行。宿主重建后，重新接入可能等待约一分钟。登录触发器和重复任务配置已检查，实际登录与重启恢复仍待验证。

文件位于：

```text
%LOCALAPPDATA%\Packages\Microsoft.Windows.ShellExperienceHost_cw5n1h2txyewy\LocalState\QuickSettingsThemeTile\
    enabled.cfg
    placement.json
    runtime\0.3.3-native\ThemeTile.dll
    runtime\0.3.3-native\ThemeTileLoader.exe
```

任务名称为 `QuickSettingsThemeTile-<当前用户 SID>`。位置文件保存前后相邻行动和索引；原生列表继续负责实际拖动。

## 兼容性与卸载

此版本使用 Windows Shell 内部模型与版本相关 ABI，目前只允许以下两种 `ControlCenter.dll` SHA-256：

```text
3E9A30BD1E92220BAEEBA90CD0AC409AE541C650019AD7C80AEC97DAF39C3261
0A9A84B0B7ED6110A410BD76150CE0DA04DC29B901838155AB7CE2D8BF2FFB51
```

这两个档案以首次 ABI 验证构建命名；运行时按 DLL 哈希、PE 指纹和所需的 PDB 身份匹配，不按主机 build 数字单独放行。Winbindex 批量索引还把这两份 DLL 字节关联到部分 26100/26200 更新，但这些主机尚未实测。2026-10-16 批量扫描的其他 36 个不同哈希仍拒绝接入。开发用 `.\\compat-check-033\\compat-scan.exe` 可检查当前系统 DLL 或离线包文件；扫描结果不会自动成为受支持档案。详细覆盖和验收边界见 [兼容矩阵](docs/compatibility-matrix.md) 与[候选发现流程](docs/compatibility-sources.md)；当前 26300.9711 证据见 [兼容性记录](docs/compatibility-26300.9711.md)。

开发者可用 [候选发现与扫描流程](docs/compatibility-sources.md) 查询 Winbindex 的 x64 元数据、从微软公共符号服务器下载并校验指定 DLL/PDB，或扫描从离线更新包提取的 DLL。公开 PDB 检查只输出匹配身份的符号线索；所有未知哈希继续拒绝加载，流程不会修改档案表或把候选加载进 Shell。

卸载会停止主题控制器、撤销订阅、移除自建磁贴，并删除计划任务与启用/位置配置。当前宿主仍映射的 DLL 可能暂时无法删除；工具会提示。等待宿主正常退出（例如正常注销）后，再执行一次卸载即可清理剩余文件。不会强制卸载仍可能被 COM 回调使用的 DLL。

## 验证状态

2026-10-06 的 0.3.0 现场检查通过了安装、重复任务去重、功能卸载和再次启用。0.3.2 为 build 26300.9711 增加精确兼容配置，0.3.3 新增统一档案检查、候选扫描和 DIA PDB 比较工具。兼容性单元检查 17 项通过；当前系统 DLL 扫描与 Beta `--status` 均识别为 `win11-x64-26300.9711`。GA 候选 26200.9168 的三个相关公开符号集合与现有档案相同，但 PDB 不含可用类布局；因此仍拒绝加载。26H1 版本级别为 GA，但 x64 更新包类别与版本渠道信息不一致；28000.3231 的相关公开符号集合也与现有档案有差异，仍保持拒绝。Beta `--status` 是只读检查，没有安装 Beta、操作或重启 Shell；请手动复验磁贴显示、切换、拖动和位置保持。

详见 [验证记录](docs/validation.md)。安装器的活动会话状态不等同于按钮实际显示或所有输入方式通过测试。

## 从源码构建

需要 Visual Studio 2022 的 x64 C++ 工具链、Windows SDK 和 C++/WinRT 头文件。已使用 MSVC 14.44.35207 / SDK 10.0.26100.0 构建。

```powershell
# 单文件启用/卸载工具，内嵌 DLL 和加载程序；静态链接 C++ 运行库
.\build.ps1 -Target product -BuildName artifacts

# 12 项隔离的事务故障检查，不修改真实系统主题
.\build.ps1 -Target backend -BuildName backend
.\backend\backend-tests.exe
```

产品输出在 `artifacts/ThemeTileSetup.exe`。开发用扫描器与检查输出可通过 `.\\build.ps1 -Target compatibility -BuildName compat-check` 构建，扫描器不会被打包进安装器。正在使用的 DLL 不可覆盖；已有组件文件必须与内嵌载荷逐字节一致。修改 DLL 后，应更新 `src/product-common.h` 的版本、`src/probe-id.h` 的产品 CLSID 和 `src/product.manifest` 的版本，使用新目录构建。

开发状态检查：

```powershell
.\artifacts\ThemeTileSetup.exe --status | Out-String
```

`enabled` 表示自动加载已配置；`shellSessionActive` 是诊断会话状态布尔值，`runtimeInstalled` 表示组件文件已安装。状态输出不包含本地路径或进程编号。这些字段均不能代替人工界面验收。
兼容字段包括 Windows build/UBR、`compatibilityStatus`、`profileId`、DLL SHA-256、PE 指纹和存在时的 PDB 标识。状态查询不会返回 DLL 的完整路径。

## 结构与许可

- `src/probe.cpp`：原生模型、磁贴模板、XAML Diagnostics 与清理。
- `src/theme-runtime.h`：原生命令、状态同步与拖动保护。
- `src/theme-backend.h`：主题读写、回滚、读回与通知。
- `src/tile-order.h`：位置保存和恢复。
- `src/product-*.cpp`：启用/卸载工具与短时加载程序。
- `src/backend-tests.cpp`：隔离的事务检查。

GPL-3.0，见 [LICENSE](LICENSE) 与 [第三方说明](THIRD-PARTY-NOTICES.md)。这是非官方实验工具；没有分发微软系统 DLL 或符号文件。
