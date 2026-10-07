#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>

namespace theme {
inline constexpr wchar_t KeyPath[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
struct State { DWORD system=0,apps=0; bool dark() const { return !system && !apps; } bool mixed() const { return system!=apps; } };
struct Key {
    HKEY value=nullptr;
    ~Key() { if (value) RegCloseKey(value); }
    Key()=default; Key(Key const&)=delete; Key& operator=(Key const&)=delete;
};
inline LSTATUS ReadValue(HKEY key,wchar_t const* name,DWORD& value) {
    DWORD type=0,size=sizeof(value);
    auto result=RegQueryValueExW(key,name,nullptr,&type,reinterpret_cast<BYTE*>(&value),&size);
    if (result!=ERROR_SUCCESS) return result;
    return type==REG_DWORD && size==sizeof(value) && value<=1 ? ERROR_SUCCESS : ERROR_INVALID_DATA;
}
inline LSTATUS Read(State& state) {
    Key key;
    auto result=RegOpenKeyExW(HKEY_CURRENT_USER,KeyPath,0,KEY_QUERY_VALUE,&key.value);
    if (result!=ERROR_SUCCESS) return result;
    result=ReadValue(key.value,L"SystemUsesLightTheme",state.system);
    return result==ERROR_SUCCESS ? ReadValue(key.value,L"AppsUseLightTheme",state.apps) : result;
}
inline bool HighContrast() {
    HIGHCONTRASTW contrast{}; contrast.cbSize=sizeof(contrast);
    // Treat an inability to query the accessibility state as unavailable.
    return !SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0) || (contrast.dwFlags&HCF_HIGHCONTRASTON);
}
struct Result { LSTATUS error=ERROR_SUCCESS; bool written=false,rollbackFailed=false,notificationsPartial=false,actualKnown=false; State actual; };
inline LSTATUS WriteValue(HKEY key,wchar_t const* name,DWORD value) {
    return RegSetValueExW(key,name,0,REG_DWORD,reinterpret_cast<BYTE const*>(&value),sizeof(value));
}
struct Notification { ULONGLONG deadline; bool partial=false; };
inline BOOL CALLBACK NotifyWindow(HWND window,LPARAM parameter) {
    auto& notification=*reinterpret_cast<Notification*>(parameter);
    for (UINT message : {WM_SETTINGCHANGE,WM_THEMECHANGED}) {
        auto now=GetTickCount64();
        if (now>=notification.deadline) { notification.partial=true; return FALSE; }
        DWORD_PTR answer=0;
        auto timeout=static_cast<UINT>((std::min)(ULONGLONG{40},notification.deadline-now));
        auto value=message==WM_SETTINGCHANGE ? reinterpret_cast<LPARAM>(L"ImmersiveColorSet") : 0;
        if (!SendMessageTimeoutW(window,message,0,value,SMTO_ABORTIFHUNG|SMTO_BLOCK,timeout,&answer)) notification.partial=true;
    }
    return TRUE;
}
inline bool Notify() {
    Notification notification{GetTickCount64()+2000};
    EnumWindows(NotifyWindow,reinterpret_cast<LPARAM>(&notification));
    return notification.partial;
}
struct RegistryStore {
    Key key;
    LSTATUS Open() { return RegOpenKeyExW(HKEY_CURRENT_USER,KeyPath,0,KEY_QUERY_VALUE|KEY_SET_VALUE,&key.value); }
    LSTATUS ReadState(State& state) {
        auto error=ReadSystem(state.system);
        return error==ERROR_SUCCESS ? ReadValue(key.value,L"AppsUseLightTheme",state.apps) : error;
    }
    LSTATUS ReadSystem(DWORD& value) { return ReadValue(key.value,L"SystemUsesLightTheme",value); }
    LSTATUS WriteSystem(DWORD value) { return WriteValue(key.value,L"SystemUsesLightTheme",value); }
    LSTATUS WriteApps(DWORD value) { return WriteValue(key.value,L"AppsUseLightTheme",value); }
};

// The transaction policy is shared by the real registry adapter and failure-injection checks.
// It never restores a value if another writer has already changed it to something else.
template<class Store> Result ToggleWithStore(Store& store) {
    Result result;
    State before;
    result.error=store.ReadState(before);
    if (result.error!=ERROR_SUCCESS) return result;
    if (before.system>1 || before.apps>1) { result.error=ERROR_INVALID_DATA; return result; }
    result.actual=before; result.actualKnown=true;
    const DWORD target=before.dark() ? 1 : 0;
    result.error=store.WriteSystem(target);
    if (result.error!=ERROR_SUCCESS) return result;
    result.error=store.WriteApps(target);
    if (result.error!=ERROR_SUCCESS) {
        DWORD current=0;
        auto readError=store.ReadSystem(current);
        if (readError==ERROR_SUCCESS && current==target)
            result.rollbackFailed=store.WriteSystem(before.system)!=ERROR_SUCCESS;
        else if (readError!=ERROR_SUCCESS) result.rollbackFailed=true;
        result.actualKnown=store.ReadState(result.actual)==ERROR_SUCCESS;
        return result;
    }
    result.written=true;
    result.error=store.ReadState(result.actual);
    result.actualKnown=result.error==ERROR_SUCCESS;
    if (result.error==ERROR_SUCCESS && (result.actual.system!=target || result.actual.apps!=target)) result.error=ERROR_RETRY;
    return result;
}
inline Result Toggle() {
    Result result;
    if (HighContrast()) { result.error=ERROR_NOT_SUPPORTED; return result; }
    RegistryStore store;
    result.error=store.Open();
    if (result.error!=ERROR_SUCCESS) return result;
    result=ToggleWithStore(store);
    // Notify on successful writes and partial-write failures, including a failed rollback.
    // A first-write failure can also notify harmlessly; errors never become a success.
    if (result.actualKnown || result.written || result.rollbackFailed)
    result.notificationsPartial=Notify();
    return result;
}
}
