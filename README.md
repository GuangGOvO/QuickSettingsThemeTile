# QuickSettingsThemeTile

在 Windows 11 原生快速设置中添加“深色模式”磁贴。沿用系统拖动、分页和磁贴模板，点击同时切换系统与应用的浅色/深色主题。

**0.3.6-native Beta 1 包含 24 个精确 `ControlCenter.dll` 指纹，新增 26100.4202/.4349/.4351 共用指纹。该精确候选 DLL 通过隔离 ABI 探针，但这几个目标构建的 Shell 接入和磁贴交互尚未实测。23H2/22H2 使用不同的旧快速操作架构，仍未适配。**

## 下载与使用

从 [Releases](https://github.com/GuangGOvO/QuickSettingsThemeTile/releases) 下载对应版本的 `ThemeTileSetup.exe`。GitHub 会为每个版本 tag 自动提供对应的源代码 ZIP；旧版本也可能有额外的 Windows ZIP。

稳定版仍是 0.3.1-native；本 Beta 含 24 个精确 DLL 指纹。运行时档案要求 DLL SHA-256、PE 元数据、DLL 内 CodeView GUID/Age 与档案完全匹配；新增 `.4202` 档案的 Initialize RVA 和 IID 另经候选 DLL 隔离 ABI 探针交叉确认。目标构建的界面验收仍待补齐；未列入档案的 DLL 会被拒绝接入。完整列表与证据见[兼容矩阵](docs/compatibility-matrix.md)。

当前预发布工具未进行代码签名，下载附件可用同页的 `SHA256SUMS.txt` 核对。

1. 双击 `ThemeTileSetup.exe`，选择“启用功能”。无需管理员权限。
2. 操作完成后关闭工具，打开任务栏右下角的快速设置。
3. 点击“深色模式”切换主题，拖动磁贴调整位置。
4. 删除功能时再次打开工具，选择“卸载功能”。当前主题和其他磁贴排列会保留。

工具使用原生 Windows 对话框，只提供启用、卸载和取消。启用后可以关闭、移动或删除安装工具；日常使用不依赖该工具运行。

从现场测试版 0.3.0 升级时，先用旧版工具卸载功能，再用新版启用。新版使用独立组件目录和产品 CLSID。旧版已映射的文件仍需宿主正常退出后用旧版工具清理；新版卸载器只清理自己的组件目录。

从 0.3.3/0.3.4/0.3.5-native Beta 升级时也要先运行旧版并选择“卸载功能”，再启用本版。每个版本使用独立的组件目录；卸载时如果 DLL 仍被 Shell 映射，按提示等待宿主退出后再清理。

## 运行方式

该功能需要 DLL 组件，不能仅靠修改原生布局或注册表恢复。安装器为当前用户配置 Windows 计划任务，在登录时以及每分钟短暂运行加载程序。加载完成或发现已有会话后，加载程序退出；没有自建常驻服务或独立监测进程。

磁贴逻辑在 `ShellHost.exe` 内持续运行。宿主重建后，重新接入可能等待约一分钟。登录触发器和重复任务配置已检查，实际登录与重启恢复仍待验证。

文件位于：

```text
%LOCALAPPDATA%\Packages\Microsoft.Windows.ShellExperienceHost_cw5n1h2txyewy\LocalState\QuickSettingsThemeTile\
    enabled.cfg
    placement.json
    runtime\0.3.6-native\ThemeTile.dll
    runtime\0.3.6-native\ThemeTileLoader.exe
```

任务名称为 `QuickSettingsThemeTile-<当前用户 SID>`。位置文件保存前后相邻行动和索引；原生列表继续负责实际拖动。

## 兼容性与卸载

此版本按精确 `ControlCenter.dll` 指纹放行，不按主机 build 数字单独放行。当前档案表有 24 个可识别指纹，覆盖已核验的 x64 正式更新组件；Winbindex 快照中另有 24H2/25H2 候选未通过或无法完成核验，26H1 x64 包属于 Insider Pre-Release，23H2/22H2 则需要不同的旧架构适配。开发用 `compat-scan.exe` 可检查当前系统 DLL 或离线包文件；扫描结果不会自动成为受支持档案。详细覆盖和验收边界见[兼容矩阵](docs/compatibility-matrix.md)与[候选发现流程](docs/compatibility-sources.md)。

开发者可用[候选发现与扫描流程](docs/compatibility-sources.md)查询 Winbindex 的 x64 元数据、从微软公共符号服务器下载 DLL/PDB，或扫描离线更新包提取的 DLL。PDB 工具目前核对 DIA 返回的 CodeView GUID/Age 并输出符号线索，但不会解析 PDB Info 流；候选 DLL 仍须通过独立 ABI 探针，所有未知哈希继续拒绝加载，流程不会修改档案表或把候选加载进 Shell。

卸载会停止主题控制器、撤销订阅、移除自建磁贴，并删除计划任务与启用/位置配置。当前宿主仍映射的 DLL 可能暂时无法删除；工具会提示。等待宿主正常退出（例如正常注销）后，再执行一次卸载即可清理剩余文件。不会强制卸载仍可能被 COM 回调使用的 DLL。

## 验证状态

0.3.6-native Beta 1 包含 24 个精确 DLL 指纹；新增 `.4202` 档案经隔离进程激活、IID、QueryInterface 和 Initialize 槽位核验，探针运行于 build 26300.9711，未调用 Initialize 或连接 ShellHost。新增档案的目标构建磁贴显示、切换、拖动和恢复仍待对应系统实测。详见[验证记录](docs/validation.md)和[兼容矩阵](docs/compatibility-matrix.md)。

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
- `src/compat-abi-probe.cpp`：开发用隔离进程 ABI 探针；不连接 Shell，也不调用 Initialize。
- `src/backend-tests.cpp`：隔离的事务检查。

GPL-3.0，见 [LICENSE](LICENSE) 与 [第三方说明](THIRD-PARTY-NOTICES.md)。这是非官方实验工具；没有分发微软系统 DLL 或符号文件。
