# QuickSettingsThemeTile

在 Windows 11 原生快速设置中添加“深色模式”磁贴。沿用系统拖动、分页和磁贴模板，点击同时切换系统与应用的浅色/深色主题。

**0.3.0-native 是预发布验证版，仅适配 Windows 11 x64、构建 26300.9457 对应的 ControlCenter.dll。安装版最终外观和交互、实际注销登录与重启恢复仍未完成验收。**

## 下载与使用

从 [Releases](https://github.com/GuangGOvO/QuickSettingsThemeTile/releases) 下载 `ThemeTileSetup.exe`，或下载包含工具、说明、许可证和对应源码的 Windows ZIP。

1. 双击 `ThemeTileSetup.exe`，选择“启用功能”。无需管理员权限。
2. 操作完成后关闭工具，打开任务栏右下角的快速设置。
3. 点击“深色模式”切换主题，拖动磁贴调整位置。
4. 删除功能时再次打开工具，选择“卸载功能”。当前主题和其他磁贴排列会保留。

工具使用原生 Windows 对话框，只提供启用、卸载和取消。启用后可以关闭、移动或删除安装工具；日常使用不依赖该工具运行。

## 运行方式

该功能需要 DLL 组件，不能仅靠修改原生布局或注册表恢复。安装器为当前用户配置 Windows 计划任务，在登录时以及每分钟短暂运行加载程序。加载完成或发现已有会话后，加载程序退出；没有自建常驻服务或独立监测进程。

磁贴逻辑在 `ShellHost.exe` 内持续运行。宿主重建后，重新接入可能等待约一分钟。登录触发器和重复任务配置已检查，实际登录与重启恢复仍待验证。

文件位于：

```text
%LOCALAPPDATA%\Packages\Microsoft.Windows.ShellExperienceHost_cw5n1h2txyewy\LocalState\QuickSettingsThemeTile\
    enabled.cfg
    placement.json
    runtime\0.3.0-native\ThemeTile.dll
    runtime\0.3.0-native\ThemeTileLoader.exe
```

任务名称为 `QuickSettingsThemeTile-<当前用户 SID>`。位置文件保存前后相邻行动和索引；原生列表继续负责实际拖动。

## 兼容性与卸载

此版本使用 Windows Shell 内部模型与版本相关 ABI，只允许以下 `ControlCenter.dll` SHA-256：

```text
3E9A30BD1E92220BAEEBA90CD0AC409AE541C650019AD7C80AEC97DAF39C3261
```

系统更新导致文件哈希变化后会拒绝接入。不要把该包用于其他 Windows 构建，也不要通过修改检查来假定兼容。

卸载会停止主题控制器、撤销订阅、移除自建磁贴，并删除计划任务与启用/位置配置。当前宿主仍映射的 DLL 可能暂时无法删除；工具会提示。等待宿主正常退出（例如正常注销）后，再执行一次卸载即可清理剩余文件。不会强制卸载仍可能被 COM 回调使用的 DLL。

## 验证状态

2026-10-06 的现场检查通过了安装、重复任务去重、功能卸载、再次启用，以及工具和加载程序退出。此前原型的首页显示、原生拖动、重开保留和真实主题切换得到人工确认；这些结果不能代替最终安装版交互验收。

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

产品输出在 `artifacts/ThemeTileSetup.exe`。正在使用的 DLL 不可覆盖；已有组件文件必须与内嵌载荷逐字节一致。修改 DLL 后，应更新 `src/product-common.h` 的版本、`src/probe-id.h` 的产品 CLSID 和 `src/product.manifest` 的版本，使用新目录构建。

开发状态检查：

```powershell
.\artifacts\ThemeTileSetup.exe --status | Out-String
```

`enabled` 表示自动加载已配置；`activeShellPids` 表示诊断会话已建立。两者均不能代替人工界面验收。

## 结构与许可

- `src/probe.cpp`：原生模型、磁贴模板、XAML Diagnostics 与清理。
- `src/theme-runtime.h`：原生命令、状态同步与拖动保护。
- `src/theme-backend.h`：主题读写、回滚、读回与通知。
- `src/tile-order.h`：位置保存和恢复。
- `src/product-*.cpp`：启用/卸载工具与短时加载程序。
- `src/backend-tests.cpp`：隔离的事务检查。

GPL-3.0，见 [LICENSE](LICENSE) 与 [第三方说明](THIRD-PARTY-NOTICES.md)。这是非官方实验工具；没有分发微软系统 DLL 或符号文件。
