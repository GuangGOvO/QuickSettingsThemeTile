param([ValidateSet('symbols','probe','backend','product','all')][string]$Target = 'all', [ValidatePattern('^[a-zA-Z0-9-]+$')][string]$BuildName = 'bin', [switch]$NativeTile, [switch]$Functional, [switch]$CleanupTile, [switch]$ResidentTile, [switch]$KeepAttached)
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
        $commonFlags=@('/nologo','/utf-8','/std:c++20','/EHsc','/W4','/MT','/DUNICODE','/D_UNICODE')
        & $compiler @commonFlags /DQS_NATIVE_ACTION_PROTOTYPE /DQS_THEME_FUNCTIONAL /DQS_RESIDENT_NATIVE /DQS_KEEP_ATTACHED /DQS_PRODUCT /LD (Join-Path $PSScriptRoot 'src\probe.cpp') /Fe:ThemeTile.dll /link windowsapp.lib ole32.lib oleaut32.lib uuid.lib advapi32.lib user32.lib shell32.lib bcrypt.lib ("/DEF:"+(Join-Path $PSScriptRoot 'src\probe.def')) /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Product DLL compilation failed.' }
        & $compiler @commonFlags /DQS_PRODUCT (Join-Path $PSScriptRoot 'src\product-loader.cpp') /Fe:ThemeTileLoader.exe /link windowsapp.lib ole32.lib uuid.lib advapi32.lib shell32.lib bcrypt.lib /SUBSYSTEM:WINDOWS /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'One-shot loader compilation failed.' }
        $dllResource=(Join-Path $buildDir 'ThemeTile.dll').Replace('\','\\')
        $loaderResource=(Join-Path $buildDir 'ThemeTileLoader.exe').Replace('\','\\')
        @('#include <windows.h>',('201 RCDATA "'+$dllResource+'"'),('202 RCDATA "'+$loaderResource+'"')) | Set-Content -LiteralPath 'payload.rc' -Encoding utf8
        $resourceCompiler=Join-Path $sdkRoot ('bin\'+$sdk.Name+'\x64\rc.exe')
        & $resourceCompiler /nologo /fo payload.res payload.rc
        if ($LASTEXITCODE) { throw 'Payload resources failed.' }
        & $compiler @commonFlags (Join-Path $PSScriptRoot 'src\product-setup.cpp') payload.res /Fe:ThemeTileSetup.exe /link windowsapp.lib ole32.lib oleaut32.lib uuid.lib advapi32.lib shell32.lib bcrypt.lib comctl32.lib taskschd.lib comsuppw.lib /SUBSYSTEM:WINDOWS /MANIFEST:EMBED ("/MANIFESTINPUT:"+(Join-Path $PSScriptRoot 'src\product.manifest')) /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Enable/uninstall tool compilation failed.' }
        [pscustomobject]@{KeepAttached=$true;ResidentTile=$true;Functional=$true;Product=$true;Loading='one-shot scheduled task';StaticRuntime=$true} | ConvertTo-Json | Set-Content -LiteralPath 'build-mode.json' -Encoding utf8
    }
    if ($Target -in 'all','symbols') {
        & $compiler /nologo /utf-8 /std:c++20 /EHsc /W4 /MD /DUNICODE /D_UNICODE (Join-Path $PSScriptRoot 'src\symbols.cpp') /Fe:symbols.exe /link /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Symbol utility compilation failed.' }
    }
    if ($Target -in 'all','backend') {
        & $compiler /nologo /utf-8 /std:c++20 /EHsc /W4 /MD /DUNICODE /D_UNICODE (Join-Path $PSScriptRoot 'src\backend-check.cpp') /Fe:backend-check.exe /link advapi32.lib user32.lib /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Theme backend compilation failed.' }
        & $compiler /nologo /utf-8 /std:c++20 /EHsc /W4 /MD /DUNICODE /D_UNICODE (Join-Path $PSScriptRoot 'src\backend-tests.cpp') /Fe:backend-tests.exe /link advapi32.lib user32.lib /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Theme backend check compilation failed.' }
    }
    if ($Target -in 'all','probe') {
        $prototypeDefines = @()
        if ($NativeTile -or $Functional -or $CleanupTile -or $ResidentTile) { $prototypeDefines += '/DQS_NATIVE_ACTION_PROTOTYPE' }
        if ($Functional) { $prototypeDefines += '/DQS_THEME_FUNCTIONAL' }
        if ($CleanupTile) { $prototypeDefines += '/DQS_CLEANUP_NATIVE' }
        if ($ResidentTile) { $prototypeDefines += '/DQS_RESIDENT_NATIVE' }
        if ($KeepAttached) { $prototypeDefines += '/DQS_KEEP_ATTACHED' }
        & $compiler /nologo /utf-8 /std:c++20 /EHsc /W4 /MD /DUNICODE /D_UNICODE @prototypeDefines /LD (Join-Path $PSScriptRoot 'src\probe.cpp') /Fe:qs-probe.dll /link windowsapp.lib ole32.lib oleaut32.lib uuid.lib advapi32.lib user32.lib ("/DEF:" + (Join-Path $PSScriptRoot 'src\probe.def')) /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Probe compilation failed.' }
        & $compiler /nologo /utf-8 /std:c++20 /EHsc /W4 /MD /DUNICODE /D_UNICODE (Join-Path $PSScriptRoot 'src\attach.cpp') /Fe:attach.exe /link ole32.lib /DYNAMICBASE /NXCOMPAT
        if ($LASTEXITCODE) { throw 'Attach utility compilation failed.' }
        [pscustomobject]@{KeepAttached=[bool]$KeepAttached;ResidentTile=[bool]$ResidentTile;Functional=[bool]$Functional} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $buildDir 'build-mode.json') -Encoding utf8
    }
} finally { Pop-Location }
