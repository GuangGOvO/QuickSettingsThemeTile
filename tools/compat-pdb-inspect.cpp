#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <dbghelp.h>
#include <wrl/client.h>
#include <dia2.h>
#include <diacreate.h>
#include <algorithm>
#include <cwctype>
#include <iostream>
#include <string>

namespace {
template<class T> using ComPtr=Microsoft::WRL::ComPtr<T>;

std::wstring Name(IDiaSymbol* symbol) {
    BSTR value=nullptr;
    if (!symbol || FAILED(symbol->get_name(&value)) || !value) return {};
    std::wstring result(value,SysStringLen(value));
    SysFreeString(value);
    return result;
}

std::wstring Clean(std::wstring value) {
    for (auto& ch:value) if (ch==L'\t' || ch==L'\r' || ch==L'\n') ch=L' ';
    return value;
}

std::wstring DisplayName(std::wstring const& value) {
    if (value.empty()) return value;
    auto bytes=WideCharToMultiByte(CP_UTF8,0,value.c_str(),-1,nullptr,0,nullptr,nullptr);
    if (bytes<=1) return value;
    std::string decorated(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8,0,value.c_str(),-1,decorated.data(),bytes,nullptr,nullptr);
    char output[16384]{};
    auto count=UnDecorateSymbolName(decorated.c_str(),output,ARRAYSIZE(output),UNDNAME_COMPLETE);
    if (!count) return value;
    auto chars=MultiByteToWideChar(CP_UTF8,0,output,static_cast<int>(count),nullptr,0);
    if (chars<=0) return value;
    std::wstring result(static_cast<size_t>(chars),L'\0');
    MultiByteToWideChar(CP_UTF8,0,output,static_cast<int>(count),result.data(),chars);
    return result;
}

bool HasFilter(std::wstring const& value,std::wstring const& filter) {
    if (filter.empty()) return true;
    size_t start=0;
    while (start<filter.size()) {
        auto end=filter.find(L'|',start);
        auto token=filter.substr(start,end==std::wstring::npos?std::wstring::npos:end-start);
        auto it=std::search(value.begin(),value.end(),token.begin(),token.end(),[](wchar_t a,wchar_t b){return std::towlower(a)==std::towlower(b);});
        if (!token.empty() && it!=value.end()) return true;
        if (end==std::wstring::npos) break;
        start=end+1;
    }
    return false;
}

void PrintMember(IDiaSymbol* member,wchar_t const* kind,std::wstring const& parent) {
    auto name=Clean(DisplayName(Name(member)));
    if (name.empty()) return;
    DWORD access=0,location=0;
    BOOL isVirtual=FALSE;
    LONG offset=-1;
    member->get_access(&access);
    member->get_locationType(&location);
    member->get_virtual(&isVirtual);
    member->get_offset(&offset);
    ComPtr<IDiaSymbol> type;
    std::wstring typeName;
    if (SUCCEEDED(member->get_type(type.GetAddressOf())) && type) typeName=Clean(Name(type.Get()));
    std::wcout<<kind<<L'\t'<<parent<<L'\t'<<name<<L'\t'<<typeName<<L'\t'<<access<<L'\t'<<location<<L'\t'<<(isVirtual?1:0)<<L'\t'<<offset<<L'\n';
}

void PrintChildren(IDiaSymbol* type,std::wstring const& parent,enum SymTagEnum tag,wchar_t const* outputKind) {
    ComPtr<IDiaEnumSymbols> children;
    if (FAILED(type->findChildren(tag,nullptr,nsNone,children.GetAddressOf())) || !children) return;
    IDiaSymbol* raw=nullptr; ULONG fetched=0;
    while (SUCCEEDED(children->Next(1,&raw,&fetched)) && fetched==1) {
        ComPtr<IDiaSymbol> child; child.Attach(raw); raw=nullptr;
        PrintMember(child.Get(),outputKind,parent);
    }
}

size_t PrintGlobals(IDiaSymbol* global,enum SymTagEnum tag,std::wstring const& filter,wchar_t const* outputKind) {
    ComPtr<IDiaEnumSymbols> symbols;
    if (FAILED(global->findChildren(tag,nullptr,nsNone,symbols.GetAddressOf())) || !symbols) return 0;
    size_t matched=0; IDiaSymbol* raw=nullptr; ULONG fetched=0;
    while (SUCCEEDED(symbols->Next(1,&raw,&fetched)) && fetched==1) {
        ComPtr<IDiaSymbol> symbol; symbol.Attach(raw); raw=nullptr;
        auto name=Clean(DisplayName(Name(symbol.Get())));
        if (name.empty() || !HasFilter(name,filter)) continue;
        DWORD rva=0; symbol->get_relativeVirtualAddress(&rva);
        std::wcout<<outputKind<<L'\t'<<name<<L'\t'<<rva<<L'\n';
        ++matched;
    }
    return matched;
}
}

int wmain(int argc,wchar_t** argv) {
    if (argc!=6) {
        std::wcerr<<L"usage: compat-pdb-inspect.exe <msdia140.dll> <candidate.pdb> <expected-guid> <expected-age> <name-substring-or-list>\n";
        return 64;
    }
    GUID expectedGuid{};
    if (FAILED(CLSIDFromString(argv[3],&expectedGuid))) { std::wcerr<<L"expected-guid is invalid\n"; return 64; }
    wchar_t* ageEnd=nullptr;
    auto expectedAge=wcstoul(argv[4],&ageEnd,10);
    if (!ageEnd || *ageEnd || expectedAge>MAXDWORD) { std::wcerr<<L"expected-age is invalid\n"; return 64; }
    auto hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if (FAILED(hr)) { std::wcerr<<L"CoInitializeEx failed: 0x"<<std::hex<<hr<<L'\n'; return 1; }
    int exitCode=1;
    do {
        ComPtr<IDiaDataSource> source;
        hr=NoRegCoCreate(argv[1],__uuidof(DiaSource),__uuidof(IDiaDataSource),reinterpret_cast<void**>(source.GetAddressOf()));
        if (FAILED(hr) || !source) { std::wcerr<<L"DIA creation failed: 0x"<<std::hex<<hr<<L'\n'; break; }
        hr=source->loadDataFromPdb(argv[2]);
        if (FAILED(hr)) { std::wcerr<<L"PDB load failed: 0x"<<std::hex<<hr<<L'\n'; break; }
        ComPtr<IDiaSession> session;
        hr=source->openSession(session.GetAddressOf());
        if (FAILED(hr) || !session) { std::wcerr<<L"DIA session failed: 0x"<<std::hex<<hr<<L'\n'; break; }
        ComPtr<IDiaSymbol> global;
        hr=session->get_globalScope(global.GetAddressOf());
        if (FAILED(hr) || !global) { std::wcerr<<L"PDB global scope failed: 0x"<<std::hex<<hr<<L'\n'; break; }
        GUID actualGuid{}; DWORD actualAge=0;
        if (FAILED(global->get_guid(&actualGuid)) || FAILED(global->get_age(&actualAge))) { std::wcerr<<L"DIA-reported PDB identity unavailable\n"; break; }
        if (std::memcmp(&actualGuid,&expectedGuid,sizeof(GUID))!=0 || actualAge!=expectedAge) {
            wchar_t actualText[40]{}; StringFromGUID2(actualGuid,actualText,ARRAYSIZE(actualText));
            std::wcerr<<L"DIA-reported PDB identity mismatch: "<<actualText<<L" age "<<actualAge<<L'\n';
            exitCode=4; break;
        }
        auto filter=std::wstring(argv[5]);
        wchar_t identityText[40]{}; StringFromGUID2(actualGuid,identityText,ARRAYSIZE(identityText));
        std::wcerr<<L"diaGuid="<<identityText<<L" diaAge="<<actualAge<<L'\n';
        ComPtr<IDiaEnumSymbols> types;
        hr=global->findChildren(SymTagUDT,nullptr,nsNone,types.GetAddressOf());
        if (FAILED(hr) || !types) { std::wcerr<<L"PDB UDT enumeration failed: 0x"<<std::hex<<hr<<L'\n'; break; }
        IDiaSymbol* raw=nullptr; ULONG fetched=0; size_t matchedTypes=0;
        while (SUCCEEDED(types->Next(1,&raw,&fetched)) && fetched==1) {
            ComPtr<IDiaSymbol> type; type.Attach(raw); raw=nullptr;
            auto name=Clean(DisplayName(Name(type.Get())));
            if (name.empty() || !HasFilter(name,filter)) continue;
            ULONGLONG size=0; DWORD udtKind=0;
            type->get_length(&size); type->get_udtKind(&udtKind);
            std::wcout<<L"TYPE\t"<<name<<L'\t'<<size<<L'\t'<<udtKind<<L'\n';
            PrintChildren(type.Get(),name,SymTagFunction,L"METHOD");
            PrintChildren(type.Get(),name,SymTagData,L"FIELD");
            ++matchedTypes;
        }
        auto publicCount=PrintGlobals(global.Get(),SymTagPublicSymbol,filter,L"PUBLIC");
        auto functionCount=PrintGlobals(global.Get(),SymTagFunction,filter,L"FUNCTION");
        std::wcerr<<L"matchedTypes="<<matchedTypes<<L" matchedPublics="<<publicCount<<L" matchedFunctions="<<functionCount<<L'\n';
        exitCode=(matchedTypes || publicCount || functionCount)?0:3;
    } while(false);
    CoUninitialize();
    return exitCode;
}
