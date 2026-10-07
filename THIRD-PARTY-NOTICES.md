# Sources and license

This research prototype is distributed under GPL-3.0. See LICENSE.

The XAML Diagnostics TAP/watcher approach and the undocumented diagnostic cleanup interface were studied from:

- Notification Center Styler, by m417z and contributors, GPL-3.0: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-notification-center-styler.wh.cpp
- UWPSpy, by m417z and contributors, GPL-3.0: https://github.com/m417z/UWPSpy

The probe code here is a reduced research implementation. It does not include either application's complete implementation, and the author of those projects has not validated this prototype.

Microsoft XAML Diagnostics, XAML schema and symbol enumeration APIs:

- https://learn.microsoft.com/en-us/windows/win32/api/xamlom/nf-xamlom-initializexamldiagnosticsex
- https://learn.microsoft.com/en-us/uwp/api/windows.ui.xaml.markup.ixamlmetadataprovider
- https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-symenumsymbols

Windows binaries and PDBs are local Microsoft components and symbols. They are not part of this source distribution. The developer needs an installed Visual Studio C++ toolchain and Windows SDK.
