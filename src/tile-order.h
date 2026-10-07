#pragma once
#include <windows.h>
#include <winrt/Windows.Data.Json.h>
#include <string>
#include <vector>
#include <algorithm>

namespace tileorder {
constexpr wchar_t SettingsKey[]=L"Software\\QuickSettingsThemeTile";
constexpr wchar_t OwnName[]=L"Codex.QuickAction.ThemeToggle";
inline std::wstring ReadPlacement() {
#ifdef QS_PRODUCT
    auto path=product::Root()/L"placement.json";
    auto file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
    if (file==INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER length{}; std::wstring value;
    if (GetFileSizeEx(file,&length) && length.QuadPart>0 && length.QuadPart<=8192) {
        std::string text(static_cast<size_t>(length.QuadPart),'\0'); DWORD read=0;
        if (ReadFile(file,text.data(),static_cast<DWORD>(text.size()),&read,nullptr) && read==text.size()) value=winrt::to_hstring(text);
    }
    CloseHandle(file); return value;
#else
    wchar_t buffer[4096]{}; DWORD bytes=sizeof(buffer);
    if (RegGetValueW(HKEY_CURRENT_USER,SettingsKey,L"Placement",RRF_RT_REG_SZ,nullptr,buffer,&bytes)!=ERROR_SUCCESS) return {};
    return buffer;
#endif
}
inline uint32_t Restore(std::vector<std::wstring> const& names,uint32_t fallback) {
    try {
        auto text=ReadPlacement(); if (text.empty()) return (std::min)(fallback,static_cast<uint32_t>(names.size()));
        auto json=winrt::Windows::Data::Json::JsonObject::Parse(text);
        for (auto const& field : {L"next",L"previous"}) {
            auto anchor=std::wstring(json.GetNamedString(field,L""));
            auto match=std::find(names.begin(),names.end(),anchor);
            if (!anchor.empty() && match!=names.end()) return static_cast<uint32_t>(match-names.begin())+(field==std::wstring_view{L"previous"}?1:0);
        }
        auto number=json.GetNamedNumber(L"index",fallback);
        if (number>=0 && number<=128) return (std::min)(static_cast<uint32_t>(number),static_cast<uint32_t>(names.size()));
    } catch (...) {}
    return (std::min)(fallback,static_cast<uint32_t>(names.size()));
}
inline LSTATUS Save(std::vector<std::wstring> const& names) {
    auto own=std::find(names.begin(),names.end(),OwnName);
    if (own==names.end()) return ERROR_NOT_FOUND;
    auto index=static_cast<size_t>(own-names.begin());
    auto json=winrt::Windows::Data::Json::JsonObject{};
    using winrt::Windows::Data::Json::JsonValue;
    json.SetNamedValue(L"index",JsonValue::CreateNumberValue(static_cast<double>(index)));
    json.SetNamedValue(L"previous",JsonValue::CreateStringValue(index?names[index-1]:L""));
    json.SetNamedValue(L"next",JsonValue::CreateStringValue(index+1<names.size()?names[index+1]:L""));
    auto text=json.Stringify();
    if (ReadPlacement()==std::wstring(text)) return ERROR_SUCCESS;
#ifdef QS_PRODUCT
    auto temporary=product::Root()/L"placement.tmp";
    auto target=product::Root()/L"placement.json";
    auto utf8=winrt::to_string(text);
    auto file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file==INVALID_HANDLE_VALUE) return GetLastError();
    DWORD written=0; bool ok=WriteFile(file,utf8.data(),static_cast<DWORD>(utf8.size()),&written,nullptr) && written==utf8.size() && FlushFileBuffers(file);
    auto error=GetLastError(); CloseHandle(file);
    if (!ok) { DeleteFileW(temporary.c_str()); return error?error:ERROR_WRITE_FAULT; }
    if (!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) { error=GetLastError(); DeleteFileW(temporary.c_str()); return error; }
    return ERROR_SUCCESS;
#else
    HKEY key=nullptr;
    auto error=RegCreateKeyExW(HKEY_CURRENT_USER,SettingsKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr);
    if (error) return error;
    error=RegSetValueExW(key,L"Placement",0,REG_SZ,reinterpret_cast<BYTE const*>(text.c_str()),static_cast<DWORD>((text.size()+1)*sizeof(wchar_t)));
    RegCloseKey(key); return error;
#endif
}
}
