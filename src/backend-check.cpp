#include "theme-backend.h"
#include <iostream>
#include <string>
int wmain(int argc,wchar_t** argv) {
    if (argc!=2 || (std::wstring(argv[1])!=L"--read" && std::wstring(argv[1])!=L"--toggle")) {
        std::wcerr << L"Usage: backend-check.exe --read | --toggle\n"; return 2;
    }
    theme::State state;
    if (std::wstring(argv[1])==L"--read") {
        auto error=theme::Read(state);
        std::cout << "{\"error\":" << error << ",\"system\":" << state.system << ",\"apps\":" << state.apps << ",\"highContrast\":" << theme::HighContrast() << "}\n";
        return error ? 1 : 0;
    }
    auto result=theme::Toggle();
    std::cout << "{\"error\":" << result.error << ",\"written\":" << result.written << ",\"actualKnown\":" << result.actualKnown << ",\"system\":" << result.actual.system << ",\"apps\":" << result.actual.apps << ",\"rollbackFailed\":" << result.rollbackFailed << ",\"notificationsPartial\":" << result.notificationsPartial << "}\n";
    return result.error ? 1 : 0;
}
