#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <iostream>
#include <string>
#include <fstream>

using InitFn = decltype(&SymInitializeW);
using LoadFn = decltype(&SymLoadModuleExW);
using EnumFn = decltype(&SymEnumSymbols);
using OptionsFn = decltype(&SymSetOptions);
using CleanupFn = decltype(&SymCleanup);
using InfoFn = decltype(&SymGetModuleInfoW64);

BOOL CALLBACK WriteSymbol(PSYMBOL_INFO symbol, ULONG, PVOID context) {
    auto& output = *static_cast<std::ofstream*>(context);
    output << std::hex << (symbol->Address - symbol->ModBase) << '\t' << symbol->Name << '\n';
    return TRUE;
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 5) { std::wcerr << L"Usage: symbols.exe <image.dll> <symbol-cache> <output.tsv> <dbghelp.dll>\n"; return 2; }
    auto dll = LoadLibraryExW(argv[4], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dll) { std::cerr << "Load dbghelp: " << GetLastError() << '\n'; return 1; }
    auto init = reinterpret_cast<InitFn>(GetProcAddress(dll, "SymInitializeW"));
    auto load = reinterpret_cast<LoadFn>(GetProcAddress(dll, "SymLoadModuleExW"));
    auto enumerate = reinterpret_cast<EnumFn>(GetProcAddress(dll, "SymEnumSymbols"));
    auto options = reinterpret_cast<OptionsFn>(GetProcAddress(dll, "SymSetOptions"));
    auto cleanup = reinterpret_cast<CleanupFn>(GetProcAddress(dll, "SymCleanup"));
    auto info = reinterpret_cast<InfoFn>(GetProcAddress(dll, "SymGetModuleInfoW64"));
    if (!init || !load || !enumerate || !options || !cleanup || !info) return 1;
    options(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
    std::wstring path = L"srv*" + std::wstring(argv[2]) + L"*https://msdl.microsoft.com/download/symbols";
    HANDLE session = GetCurrentProcess();
    if (!init(session, path.c_str(), FALSE)) { std::cerr << "Init: " << GetLastError() << '\n'; return 1; }
    auto base = load(session, nullptr, argv[1], nullptr, 0, 0, nullptr, 0);
    if (!base) { std::cerr << "Load: " << GetLastError() << '\n'; cleanup(session); return 1; }
    std::ofstream output(argv[3], std::ios::binary);
    if (!output || !enumerate(session, base, nullptr, WriteSymbol, &output)) { std::cerr << "Enum: " << GetLastError() << '\n'; cleanup(session); return 1; }
    IMAGEHLP_MODULEW64 module{};
    module.SizeOfStruct = sizeof(module);
    if (info(session, base, &module)) std::wcout << L"PDB: " << module.LoadedPdbName << L"\nSymbol type: " << module.SymType << '\n';
    cleanup(session);
    FreeLibrary(dll);
    return 0;
}
