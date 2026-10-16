param([ValidateSet('symbols','probe','backend','product','compatibility','abi-probe','pdb-inspector','all')][string]$Target = 'all', [ValidatePattern('^[a-zA-Z0-9-]+$')][string]$BuildName = 'bin', [switch]$NativeTile, [switch]$Functional, [switch]$CleanupTile, [switch]$ResidentTile, [switch]$KeepAttached)
$ErrorActionPreference = 'Stop'
if ($KeepAttached) { $ResidentTile=$true }
if ($CleanupTile -and ($NativeTile -or $Functional -or $ResidentTile)) { throw 'CleanupTile cannot be combined with insertion modes.' }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ toolchain not found.' }
$msvc = Get-ChildItem -LiteralPath (Join-Path $vs 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
$sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$sdk = Get-ChildItem -LiteralPath (Join-Path $sdkRoot 'Include') -Directory | Sort-Object Name -Descending | Select-Object -First 1
$env:PATH=(Join-Path $sdkRoot ('bin\'+$sdk.Name+'\x64'))+';'+$env:PATH
$compiler = Join-Path $msvc.FullName 'bin\Hostx64\x64\cl.exe'
$env:INCLUDE = @((Join-Path $msvc.FullName 'include'), (Join-Path $sdk.FullName 'ucrt'), (Join-Path $sdk.FullName 'shared'), (Join-Path $sdk.FullName 'um'), (Join-Path $sdk.FullName 'winrt'), (Join-Path $sdk.FullName 'cppwinrt')) -join ';'
$env:LIB = @((Join-Path $msvc.FullName 'lib\x64'), (Join-Path $sdkRoot "Lib\$($sdk.Name)\ucrt\x64"), (Join-Path $sdkRoot "Lib\$($sdk.Name)\um\x64")) -join ';'
$buildDir = Join-Path $PSScriptRoot $BuildName
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
Push-Location -LiteralPath $buildDir
try {
    if ($Target -eq 'product') {
        # C++/WinRT source-location diagnostics can embed absolute header paths.
        $commonFlags=@('/nologo','/utf-8','/std:c++20','/EHsc','/W4','/MT','/DUNICODE','/D_UNICODE','/DWINRT_NO_SOURCE_LOCATION')
        & $compiler @commonFlags /DQS_NATIVE_ACTION_PROTOTYPE /DQS_THEME_FUNCTIONAL /DQS_RESIDENT_NATIVE /DQS_KEEP_ATTACHED /DQS_PRODUCT /LD '..\src\probe.cpp' /Fe:ThemeTile.dll /link windowsapp.lib ole32.lib oleaut32.lib uuid.lib advapi32.lib user32.lib shell32.lib bcrypt.lib '/DEF:..\src\probe.def' /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Product DLL compilation failed.' }
        & $compiler @commonFlags /DQS_PRODUCT '..\src\product-loader.cpp' /Fe:ThemeTileLoader.exe /link windowsapp.lib ole32.lib uuid.lib advapi32.lib shell32.lib bcrypt.lib /SUBSYSTEM:WINDOWS /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'One-shot loader compilation failed.' }
        $dllResource='ThemeTile.dll'
        $loaderResource='ThemeTileLoader.exe'
        @('#include <windows.h>',('201 RCDATA "'+$dllResource+'"'),('202 RCDATA "'+$loaderResource+'"')) | Set-Content -LiteralPath 'payload.rc' -Encoding utf8
        $resourceCompiler=Join-Path $sdkRoot ('bin\'+$sdk.Name+'\x64\rc.exe')
        & $resourceCompiler /nologo /fo payload.res payload.rc
        if ($LASTEXITCODE) { throw 'Payload resources failed.' }
        & $compiler @commonFlags '..\src\product-setup.cpp' payload.res /Fe:ThemeTileSetup.exe /link windowsapp.lib ole32.lib oleaut32.lib uuid.lib advapi32.lib shell32.lib bcrypt.lib comctl32.lib taskschd.lib comsuppw.lib /SUBSYSTEM:WINDOWS /MANIFEST:EMBED '/MANIFESTINPUT:..\src\product.manifest' /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Enable/uninstall tool compilation failed.' }
        [pscustomobject]@{KeepAttached=$true;ResidentTile=$true;Functional=$true;Product=$true;Loading='one-shot scheduled task';StaticRuntime=$true} | ConvertTo-Json | Set-Content -LiteralPath 'build-mode.json' -Encoding utf8
    }
    if ($Target -in 'all','symbols') {
        & $compiler /nologo /utf-8 /std:c++20 /EHsc /W4 /MD /DUNICODE /D_UNICODE '..\src\symbols.cpp' /Fe:symbols.exe /link /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Symbol utility compilation failed.' }
    }
    if ($Target -in 'all','backend') {
        & $compiler /nologo /utf-8 /std:c++20 /EHsc /W4 /MD /DUNICODE /D_UNICODE '..\src\backend-check.cpp' /Fe:backend-check.exe /link advapi32.lib user32.lib /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Theme backend compilation failed.' }
        & $compiler /nologo /utf-8 /std:c++20 /EHsc /W4 /MD /DUNICODE /D_UNICODE '..\src\backend-tests.cpp' /Fe:backend-tests.exe /link advapi32.lib user32.lib /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Theme backend check compilation failed.' }
    }
    if ($Target -in 'all','compatibility') {
        & $compiler /nologo /utf-8 /std:c++20 /EHsc /W4 /MT /DUNICODE /D_UNICODE '..\src\compatibility-tests.cpp' /Fe:compatibility-tests.exe /link bcrypt.lib ole32.lib /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Compatibility tests compilation failed.' }
        & $compiler /nologo /utf-8 /std:c++20 /EHsc /W4 /MT /DUNICODE /D_UNICODE '..\src\compat-scan.cpp' /Fe:compat-scan.exe /link bcrypt.lib ole32.lib advapi32.lib /SUBSYSTEM:CONSOLE /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Compatibility scanner compilation failed.' }
        & '.\compatibility-tests.exe'
        if ($LASTEXITCODE) { throw 'Compatibility tests failed.' }
    }
    if ($Target -eq 'abi-probe') {
        & $compiler /nologo /utf-8 /std:c++20 /EHsc /W4 /MT /DUNICODE /D_UNICODE '..\src\compat-abi-probe.cpp' /Fe:compat-abi-probe.exe /link windowsapp.lib ole32.lib bcrypt.lib /SUBSYSTEM:CONSOLE /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Compatibility ABI probe compilation failed.' }
    }
    if ($Target -eq 'pdb-inspector') {
        $diaRoot=Join-Path $vs 'DIA SDK'
        if (!(Test-Path -LiteralPath (Join-Path $diaRoot 'include\dia2.h')) -or !(Test-Path -LiteralPath (Join-Path $diaRoot 'lib\amd64\diaguids.lib'))) { throw 'Visual Studio DIA SDK not found.' }
        & $compiler /nologo /utf-8 /std:c++20 /EHsc /W4 /MT "/I$(Join-Path $diaRoot 'include')" '..\tools\compat-pdb-inspect.cpp' /Fe:compat-pdb-inspect.exe /link "/LIBPATH:$(Join-Path $diaRoot 'lib\amd64')" diaguids.lib ole32.lib oleaut32.lib advapi32.lib dbghelp.lib /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Compatibility PDB inspector compilation failed.' }
    }
    if ($Target -in 'all','probe') {
        $prototypeDefines = @()
        if ($NativeTile -or $Functional -or $CleanupTile -or $ResidentTile) { $prototypeDefines += '/DQS_NATIVE_ACTION_PROTOTYPE' }
        if ($Functional) { $prototypeDefines += '/DQS_THEME_FUNCTIONAL' }
        if ($CleanupTile) { $prototypeDefines += '/DQS_CLEANUP_NATIVE' }
        if ($ResidentTile) { $prototypeDefines += '/DQS_RESIDENT_NATIVE' }
        if ($KeepAttached) { $prototypeDefines += '/DQS_KEEP_ATTACHED' }
        & $compiler /nologo /utf-8 /std:c++20 /EHsc /W4 /MD /DUNICODE /D_UNICODE @prototypeDefines /LD '..\src\probe.cpp' /Fe:qs-probe.dll /link windowsapp.lib ole32.lib oleaut32.lib uuid.lib advapi32.lib user32.lib '/DEF:..\src\probe.def' /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Probe compilation failed.' }
        & $compiler /nologo /utf-8 /std:c++20 /EHsc /W4 /MD /DUNICODE /D_UNICODE '..\src\attach.cpp' /Fe:attach.exe /link ole32.lib /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Attach utility compilation failed.' }
        [pscustomobject]@{KeepAttached=[bool]$KeepAttached;ResidentTile=[bool]$ResidentTile;Functional=[bool]$Functional} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $buildDir 'build-mode.json') -Encoding utf8
    }
} finally { Pop-Location }
