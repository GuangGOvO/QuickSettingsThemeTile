#include "product-common.h"
#include <xamlom.h>
#include "probe-id.h"

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    try {
        if (!product::Enabled()) return 0;
        auto mutex=CreateMutexW(nullptr,TRUE,L"Local\\QuickSettingsThemeTile.OneShotLoader");
        if (!mutex) return 1;
        if (GetLastError()==ERROR_ALREADY_EXISTS) { CloseHandle(mutex); return 0; }
        struct Guard { HANDLE value; ~Guard() { ReleaseMutex(value); CloseHandle(value); } } guard{mutex};
        if (!product::Compatible()) return 2;
        auto dll=product::Runtime()/product::DllName;
        if (!std::filesystem::is_regular_file(dll)) return 3;
        auto xaml=LoadLibraryExW(L"Windows.UI.Xaml.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!xaml) return 4;
        auto initialize=reinterpret_cast<decltype(&InitializeXamlDiagnosticsEx)>(GetProcAddress(xaml,"InitializeXamlDiagnosticsEx"));
        if (!initialize) { FreeLibrary(xaml); return 4; }
        auto logs=product::LocalData()/L"Packages"/L"Microsoft.Windows.ShellExperienceHost_cw5n1h2txyewy"/L"TempState"/L"QuickSettingsThemeProbe";
        std::filesystem::create_directories(logs);
        auto deadline=GetTickCount64()+8000;
        HRESULT result=HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        do {
            if (!product::Enabled()) { FreeLibrary(xaml); return 0; }
            for (auto pid : product::Hosts()) {
                if (product::Active(pid)) { FreeLibrary(xaml); return 0; }
                if (!product::ModuleLoaded(pid,L"ControlCenter.dll")) continue;
                GUID guid{}; CoCreateGuid(&guid); wchar_t unique[40]{}; StringFromGUID2(guid,unique,40);
                auto log=logs/(L"installed-"+std::to_wstring(pid)+L"-"+unique+L".log");
                for (unsigned connection=1; connection<=10000; ++connection) {
                    auto endpoint=L"VisualDiagConnection"+std::to_wstring(connection);
                    result=initialize(endpoint.c_str(),pid,L"",dll.c_str(),ProbeClsid,log.c_str());
                    if (result!=HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) break;
                }
                if (SUCCEEDED(result)) {
                    // The diagnostics API can succeed before the target accepts SetSite.
                    // Confirm the target's session rather than treating HRESULT alone as a load.
                    do {
                        if (product::Active(pid)) { FreeLibrary(xaml); return 0; }
                        Sleep(100);
                    } while (GetTickCount64()<deadline && product::Enabled());
                    FreeLibrary(xaml); return 7;
                }
            }
            Sleep(250);
        } while (GetTickCount64()<deadline);
        FreeLibrary(xaml);
        return result==HRESULT_FROM_WIN32(ERROR_NOT_FOUND)?0:5;
    } catch (...) { return 6; }
}
