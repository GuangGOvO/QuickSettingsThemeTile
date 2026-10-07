#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xamlom.h>
#include <iostream>
#include <string>
#include "probe-id.h"

int wmain(int argc, wchar_t** argv) {
    if (argc==3 && std::wstring(argv[1])==L"--stop") {
        auto eventName=L"Local\\QuickSettingsThemeProbe.Stop."+std::wstring(argv[2]);
        auto event=OpenEventW(EVENT_MODIFY_STATE,FALSE,eventName.c_str());
        if (!event) { std::wcerr << L"No active stoppable session: " << GetLastError() << '\n'; return 1; }
        SetEvent(event); CloseHandle(event); std::wcout << L"Stop requested.\n"; return 0;
    }
    if (argc != 4) { std::wcerr << L"Usage: attach.exe <Shell PID> <absolute probe DLL> <absolute log file>\n"; return 2; }
    wchar_t* end = nullptr;
    auto pid = wcstoul(argv[1], &end, 10);
    if (!pid || !end || *end) return 2;
    HANDLE target = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!target) { std::wcerr << L"OpenProcess: " << GetLastError() << '\n'; return 1; }
    wchar_t image[32768]; DWORD length = ARRAYSIZE(image);
    bool valid = QueryFullProcessImageNameW(target,0,image,&length);
    CloseHandle(target);
    std::wstring name = valid ? image : L"";
    auto slash = name.find_last_of(L"\\/");
    name = name.substr(slash == std::wstring::npos ? 0 : slash + 1);
    if (_wcsicmp(name.c_str(),L"ShellHost.exe") && _wcsicmp(name.c_str(),L"ShellExperienceHost.exe")) {
        std::wcerr << L"Target must be ShellHost.exe or ShellExperienceHost.exe.\n"; return 2;
    }
    auto module = LoadLibraryExW(L"Windows.UI.Xaml.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) { std::wcerr << L"Load XAML: " << GetLastError() << '\n'; return 1; }
    auto initialize = reinterpret_cast<decltype(&InitializeXamlDiagnosticsEx)>(GetProcAddress(module,"InitializeXamlDiagnosticsEx"));
    if (!initialize) return 1;
    HRESULT result = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    for (unsigned connection = 1; connection <= 10000; ++connection) {
        auto endpoint = L"VisualDiagConnection" + std::to_wstring(connection);
        result = initialize(endpoint.c_str(),pid,L"",argv[2],ProbeClsid,argv[3]);
        if (result != HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) {
            std::wcout << L"Endpoint: " << endpoint << L"\n"; break;
        }
    }
    std::wcout << L"Attach HRESULT: 0x" << std::hex << static_cast<unsigned long>(result) << L"\n";
    FreeLibrary(module);
    return FAILED(result) ? 1 : 0;
}
