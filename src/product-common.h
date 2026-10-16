#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <sddl.h>
#include <winrt/base.h>
#include "compatibility.h"
#include <filesystem>
#include <string>
#include <vector>
#include <cstdio>

namespace product {
constexpr wchar_t SettingsKey[]=L"Software\\QuickSettingsThemeTile";
constexpr wchar_t Version[]=L"0.3.5-native";
constexpr wchar_t ActivePrefix[]=L"Local\\QuickSettingsThemeTile.Active.";
constexpr wchar_t StopPrefix[]=L"Local\\QuickSettingsThemeProbe.Stop.";
constexpr wchar_t DllName[]=L"ThemeTile.dll";
constexpr wchar_t HelperName[]=L"ThemeTileLoader.exe";
inline std::filesystem::path LocalData() {
    PWSTR result=nullptr;
    winrt::check_hresult(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&result));
    std::filesystem::path path{result}; CoTaskMemFree(result); return path;
}
inline std::filesystem::path Root() {
#ifdef QS_PRODUCT
    HMODULE module=nullptr; wchar_t location[32768]{};
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&Root),&module)
        && GetModuleFileNameW(module,location,ARRAYSIZE(location))) {
        auto directory=std::filesystem::path(location).parent_path();
        if (directory.filename()==Version && directory.parent_path().filename()==L"runtime") return directory.parent_path().parent_path();
    }
#endif
    return LocalData()/L"Packages"/L"Microsoft.Windows.ShellExperienceHost_cw5n1h2txyewy"/L"LocalState"/L"QuickSettingsThemeTile";
}
inline std::filesystem::path Runtime() { return Root()/L"runtime"/Version; }
inline DWORD Enabled(DWORD* errorResult=nullptr) {
    // Use a shared file in the Shell package's state directory. Packaged processes
    // can observe different virtualized registry/AppData views despite sharing a SID.
    auto path=Root()/L"enabled.cfg";
    auto file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
    if (file==INVALID_HANDLE_VALUE) { if (errorResult) *errorResult=GetLastError(); return 0; }
    DWORD value=0,read=0; LARGE_INTEGER length{};
    bool ok=GetFileSizeEx(file,&length) && length.QuadPart==sizeof(value) && ReadFile(file,&value,sizeof(value),&read,nullptr) && read==sizeof(value);
    auto error=ok?ERROR_SUCCESS:GetLastError(); CloseHandle(file);
    if (errorResult) *errorResult=error;
    return ok && value==1?1:0;
}
inline void SetEnabled(bool enabled) {
    auto path=Root()/L"enabled.cfg";
    if (!enabled) {
        if (!DeleteFileW(path.c_str()) && GetLastError()!=ERROR_FILE_NOT_FOUND && GetLastError()!=ERROR_PATH_NOT_FOUND) winrt::throw_last_error();
        return;
    }
    auto file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file==INVALID_HANDLE_VALUE) winrt::throw_last_error();
    DWORD value=1,written=0; bool ok=WriteFile(file,&value,sizeof(value),&written,nullptr) && written==sizeof(value) && FlushFileBuffers(file);
    auto error=GetLastError(); CloseHandle(file);
    if (!ok) winrt::throw_hresult(HRESULT_FROM_WIN32(error?error:ERROR_WRITE_FAULT));
}
inline std::wstring Sid() {
    HANDLE token=nullptr; if (!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) winrt::throw_last_error();
    DWORD size=0; GetTokenInformation(token,TokenUser,nullptr,0,&size);
    std::vector<BYTE> data(size);
    if (!GetTokenInformation(token,TokenUser,data.data(),size,&size)) { auto error=GetLastError(); CloseHandle(token); winrt::throw_hresult(HRESULT_FROM_WIN32(error)); }
    CloseHandle(token); PWSTR sid=nullptr;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid,&sid)) winrt::throw_last_error();
    std::wstring value{sid}; LocalFree(sid); return value;
}
inline std::wstring TaskName() { return L"QuickSettingsThemeTile-"+Sid(); }
inline std::vector<DWORD> Hosts() {
    std::vector<DWORD> result; DWORD ownSession=0; ProcessIdToSessionId(GetCurrentProcessId(),&ownSession);
    auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); if (snapshot==INVALID_HANDLE_VALUE) return result;
    PROCESSENTRY32W entry{sizeof(entry)};
    if (Process32FirstW(snapshot,&entry)) do {
        if (_wcsicmp(entry.szExeFile,L"ShellHost.exe")) continue;
        DWORD session=0;
        if (ProcessIdToSessionId(entry.th32ProcessID,&session) && session==ownSession) result.push_back(entry.th32ProcessID);
    } while (Process32NextW(snapshot,&entry));
    CloseHandle(snapshot); return result;
}
inline bool ModuleLoaded(DWORD pid,wchar_t const* name) {
    auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
    if (snapshot==INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W entry{sizeof(entry)}; bool found=false;
    if (Module32FirstW(snapshot,&entry)) do { if (!_wcsicmp(entry.szModule,name)) { found=true; break; } } while (Module32NextW(snapshot,&entry));
    CloseHandle(snapshot); return found;
}
inline bool Active(DWORD pid) {
    auto event=OpenEventW(SYNCHRONIZE,FALSE,(std::wstring(ActivePrefix)+std::to_wstring(pid)).c_str());
    if (!event) return false;
    bool active=WaitForSingleObject(event,0)==WAIT_TIMEOUT; CloseHandle(event); return active;
}
inline bool Stop(DWORD pid) {
    auto event=OpenEventW(EVENT_MODIFY_STATE,FALSE,(std::wstring(StopPrefix)+std::to_wstring(pid)).c_str());
    if (!event) return false; bool success=SetEvent(event); CloseHandle(event); return success;
}
inline compatibility::Result InspectCompatibility() { return compatibility::InspectSystemDll(); }
inline bool Compatible() { return InspectCompatibility().supported(); }
inline std::wstring SupportedProfilesSummary() {
    return std::to_wstring(compatibility::Profiles.size())+L" 个 Windows 11 x64 精确 DLL 指纹";
}
inline void Output(std::wstring const& text) {
    auto utf8=winrt::to_string(text+L"\n"); DWORD written=0;
    auto handle=GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle && handle!=INVALID_HANDLE_VALUE) WriteFile(handle,utf8.data(),static_cast<DWORD>(utf8.size()),&written,nullptr);
}
}
