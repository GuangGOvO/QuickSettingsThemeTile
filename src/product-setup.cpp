#include "product-common.h"
#include <taskschd.h>
#include <comdef.h>
#include <commctrl.h>
#include <shellapi.h>
#undef GetCurrentTime
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace {
using winrt::check_hresult;
using Path=std::filesystem::path;
winrt::com_ptr<ITaskService> Scheduler() {
    winrt::com_ptr<ITaskService> service;
    check_hresult(CoCreateInstance(CLSID_TaskScheduler,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(service.put())));
    check_hresult(service->Connect(_variant_t{},_variant_t{},_variant_t{},_variant_t{})); return service;
}
winrt::com_ptr<ITaskFolder> Folder(ITaskService* service) {
    winrt::com_ptr<ITaskFolder> folder; check_hresult(service->GetFolder(_bstr_t(L"\\"),folder.put())); return folder;
}
winrt::com_ptr<IRegisteredTask> Existing(ITaskFolder* folder) {
    winrt::com_ptr<IRegisteredTask> task;
    auto hr=folder->GetTask(_bstr_t(product::TaskName().c_str()),task.put());
    if (hr==HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || hr==HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)) return {};
    check_hresult(hr); return task;
}
bool Owned(IRegisteredTask* task) {
    if (!task) return false;
    winrt::com_ptr<ITaskDefinition> definition; check_hresult(task->get_Definition(definition.put()));
    winrt::com_ptr<IActionCollection> actions; check_hresult(definition->get_Actions(actions.put()));
    LONG count=0; check_hresult(actions->get_Count(&count)); if (count!=1) return false;
    winrt::com_ptr<IAction> action; check_hresult(actions->get_Item(1,action.put()));
    auto exec=action.try_as<IExecAction>(); if (!exec) return false;
    BSTR value=nullptr; check_hresult(exec->get_Path(&value)); std::wstring path{value?value:L""}; SysFreeString(value);
    path=Path(path).lexically_normal().wstring();
    auto root=(product::Root()/L"runtime").lexically_normal().wstring()+L"\\";
    return path.size()>root.size() && !_wcsnicmp(path.c_str(),root.c_str(),root.size()) && !_wcsicmp(Path(path).filename().c_str(),product::HelperName);
}
void Register() {
    auto service=Scheduler(); auto folder=Folder(service.get()); auto existing=Existing(folder.get());
    if (existing && !Owned(existing.get())) winrt::throw_hresult(HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
    winrt::com_ptr<ITaskDefinition> definition; check_hresult(service->NewTask(0,definition.put()));
    winrt::com_ptr<IRegistrationInfo> info; check_hresult(definition->get_RegistrationInfo(info.put()));
    check_hresult(info->put_Description(_bstr_t(L"Loads the Quick Settings theme tile, then exits. No resident installer or custom service.")));
    winrt::com_ptr<IPrincipal> principal; check_hresult(definition->get_Principal(principal.put()));
    auto sid=product::Sid(); check_hresult(principal->put_UserId(_bstr_t(sid.c_str())));
    check_hresult(principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN)); check_hresult(principal->put_RunLevel(TASK_RUNLEVEL_LUA));
    winrt::com_ptr<ITaskSettings> settings; check_hresult(definition->get_Settings(settings.put()));
    check_hresult(settings->put_DisallowStartIfOnBatteries(VARIANT_FALSE)); check_hresult(settings->put_StopIfGoingOnBatteries(VARIANT_FALSE));
    check_hresult(settings->put_StartWhenAvailable(VARIANT_TRUE)); check_hresult(settings->put_MultipleInstances(TASK_INSTANCES_IGNORE_NEW));
    check_hresult(settings->put_ExecutionTimeLimit(_bstr_t(L"PT20S")));
    winrt::com_ptr<ITriggerCollection> triggers; check_hresult(definition->get_Triggers(triggers.put()));
    winrt::com_ptr<ITrigger> base; check_hresult(triggers->Create(TASK_TRIGGER_LOGON,base.put()));
    auto logon=base.as<ILogonTrigger>(); check_hresult(logon->put_UserId(_bstr_t(sid.c_str())));
    base=nullptr; check_hresult(triggers->Create(TASK_TRIGGER_TIME,base.put()));
    SYSTEMTIME now{}; GetLocalTime(&now); wchar_t boundary[40]{};
    swprintf_s(boundary,L"%04u-%02u-%02uT%02u:%02u:%02u",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond);
    check_hresult(base->put_StartBoundary(_bstr_t(boundary)));
    winrt::com_ptr<IRepetitionPattern> repetition; check_hresult(base->get_Repetition(repetition.put()));
    check_hresult(repetition->put_Interval(_bstr_t(L"PT1M"))); check_hresult(repetition->put_StopAtDurationEnd(VARIANT_FALSE));
    winrt::com_ptr<IActionCollection> actions; check_hresult(definition->get_Actions(actions.put()));
    winrt::com_ptr<IAction> action; check_hresult(actions->Create(TASK_ACTION_EXEC,action.put()));
    auto exec=action.as<IExecAction>(); auto helper=product::Runtime()/product::HelperName;
    check_hresult(exec->put_Path(_bstr_t(helper.c_str())));
    check_hresult(exec->put_WorkingDirectory(_bstr_t(product::Runtime().c_str())));
    winrt::com_ptr<IRegisteredTask> registered;
    check_hresult(folder->RegisterTaskDefinition(_bstr_t(product::TaskName().c_str()),definition.get(),TASK_CREATE_OR_UPDATE,
        _variant_t(sid.c_str()),_variant_t{},TASK_LOGON_INTERACTIVE_TOKEN,_variant_t{},registered.put()));
}
void DeleteTask() {
    auto service=Scheduler(); auto folder=Folder(service.get()); auto task=Existing(folder.get());
    if (!task) return;
    if (!Owned(task.get())) winrt::throw_hresult(HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
    task->Stop(0);
    check_hresult(folder->DeleteTask(_bstr_t(product::TaskName().c_str()),0));
}
void SafeFolders(bool create=true) {
    // Every file operation uses fixed paths below this user's explicit product directory.
    for (auto const& path : {product::Root(),product::Root()/L"runtime",product::Runtime()}) {
        auto attributes=GetFileAttributesW(path.c_str());
        if (attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT)) winrt::throw_hresult(HRESULT_FROM_WIN32(ERROR_REPARSE_TAG_INVALID));
        if (create) std::filesystem::create_directories(path);
    }
}
void Extract(HINSTANCE instance,int id,Path const& path) {
    auto resource=FindResourceW(instance,MAKEINTRESOURCEW(id),RT_RCDATA); if (!resource) winrt::throw_last_error();
    DWORD size=SizeofResource(instance,resource); auto data=LockResource(LoadResource(instance,resource)); if (!data || !size) winrt::throw_hresult(E_FAIL);
    if (std::filesystem::exists(path)) {
        auto file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
        if (file==INVALID_HANDLE_VALUE) winrt::throw_last_error();
        LARGE_INTEGER length{}; bool same=GetFileSizeEx(file,&length) && length.QuadPart==size; std::vector<BYTE> buffer(same?size:0); DWORD read=0;
        if (same) same=ReadFile(file,buffer.data(),size,&read,nullptr) && read==size && !memcmp(buffer.data(),data,size);
        CloseHandle(file); if (!same) winrt::throw_hresult(HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH)); return;
    }
    auto file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr); if (file==INVALID_HANDLE_VALUE) winrt::throw_last_error();
    DWORD written=0; bool ok=WriteFile(file,data,size,&written,nullptr) && written==size && FlushFileBuffers(file);
    auto error=GetLastError(); CloseHandle(file);
    if (!ok) { DeleteFileW(path.c_str()); winrt::throw_hresult(HRESULT_FROM_WIN32(error?error:ERROR_WRITE_FAULT)); }
}
DWORD LoadOnce() {
    auto helper=product::Runtime()/product::HelperName; auto command=L"\""+helper.wstring()+L"\"";
    STARTUPINFOW startup{sizeof(startup)}; startup.dwFlags=STARTF_USESHOWWINDOW; startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(helper.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,product::Runtime().c_str(),&startup,&process)) winrt::throw_last_error();
    CloseHandle(process.hThread); DWORD result=WAIT_TIMEOUT;
    if (WaitForSingleObject(process.hProcess,12000)==WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess,&result);
    CloseHandle(process.hProcess); return result;
}
bool Enable(HINSTANCE instance) {
    if (!product::Compatible()) winrt::throw_hresult(HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));
    SafeFolders(); Extract(instance,201,product::Runtime()/product::DllName); Extract(instance,202,product::Runtime()/product::HelperName);
    Register();
    try { product::SetEnabled(true); } catch (...) { DeleteTask(); throw; }
    auto result=LoadOnce(); bool active=false; for (auto pid : product::Hosts()) active=active || product::Active(pid);
    product::Output(L"{\"enabled\":true,\"loaderExit\":"+std::to_wstring(result)+L",\"nativeActive\":"+(active?L"true":L"false")+L"}");
    return active;
}
void CleanNativeLayout() {
    using namespace winrt::Windows::Data::Json;
    constexpr wchar_t keyPath[]=L"Control Panel\\Quick Actions\\Control Center";
    for (unsigned attempt=0; attempt<3; ++attempt) {
        DWORD bytes=0;
        auto error=RegGetValueW(HKEY_CURRENT_USER,keyPath,L"UserLayoutPaginated",RRF_RT_REG_SZ,nullptr,nullptr,&bytes);
        if (error==ERROR_FILE_NOT_FOUND) return;
        winrt::check_win32(error); if (bytes>1024*1024) winrt::throw_hresult(HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
        std::vector<wchar_t> buffer(bytes/sizeof(wchar_t)+1);
        winrt::check_win32(RegGetValueW(HKEY_CURRENT_USER,keyPath,L"UserLayoutPaginated",RRF_RT_REG_SZ,nullptr,buffer.data(),&bytes));
        std::wstring original{buffer.data()}; auto groups=JsonArray::Parse(original); bool changed=false;
        for (auto const& group : groups) {
            auto object=group.GetObject(); if (object.GetNamedString(L"Name",L"")!=L"Toggles") continue;
            auto actions=object.GetNamedArray(L"QuickActions");
            for (uint32_t i=actions.Size(); i>0; --i) {
                if (actions.GetAt(i-1).GetObject().GetNamedString(L"FriendlyName",L"")==L"Codex.QuickAction.ThemeToggle") { actions.RemoveAt(i-1); changed=true; }
            }
        }
        if (!changed) return;
        DWORD currentBytes=static_cast<DWORD>(buffer.size()*sizeof(wchar_t));
        if (RegGetValueW(HKEY_CURRENT_USER,keyPath,L"UserLayoutPaginated",RRF_RT_REG_SZ,nullptr,buffer.data(),&currentBytes)!=ERROR_SUCCESS || original!=buffer.data()) continue;
        HKEY key=nullptr; winrt::check_win32(RegOpenKeyExW(HKEY_CURRENT_USER,keyPath,0,KEY_SET_VALUE,&key));
        auto text=groups.Stringify(); error=RegSetValueExW(key,L"UserLayoutPaginated",0,REG_SZ,reinterpret_cast<BYTE const*>(text.c_str()),static_cast<DWORD>((text.size()+1)*sizeof(wchar_t)));
        RegCloseKey(key); winrt::check_win32(error); return;
    }
    winrt::throw_hresult(HRESULT_FROM_WIN32(ERROR_RETRY));
}
bool Uninstall() {
    SafeFolders(false);
    product::SetEnabled(false); DeleteTask();
    auto hosts=product::Hosts(); for (auto pid : hosts) product::Stop(pid);
    auto deadline=GetTickCount64()+12000; bool active=false;
    do { active=false; for (auto pid : hosts) active=active || product::Active(pid); if (!active) break; Sleep(100); } while (GetTickCount64()<deadline);
    if (active) winrt::throw_hresult(HRESULT_FROM_WIN32(ERROR_TIMEOUT));
    CleanNativeLayout(); SafeFolders(false);
    RegDeleteTreeW(HKEY_CURRENT_USER,product::SettingsKey);
    bool pending=false;
    // Never recursively remove user files, and never move/delete a system directory.
    for (auto const& name : {L"placement.json",L"placement.tmp"}) DeleteFileW((product::Root()/name).c_str());
    for (auto const& name : {product::DllName,product::HelperName}) {
        auto path=product::Runtime()/name;
        if (!DeleteFileW(path.c_str()) && GetLastError()!=ERROR_FILE_NOT_FOUND && GetLastError()!=ERROR_PATH_NOT_FOUND) pending=true;
    }
    for (auto const& path : {product::Runtime(),product::Root()/L"runtime",product::Root()}) RemoveDirectoryW(path.c_str());
    product::Output(pending?L"{\"enabled\":false,\"pendingMappedFileCleanup\":true}":L"{\"enabled\":false,\"pendingMappedFileCleanup\":false}");
    return pending;
}
void Status() {
    using namespace winrt::Windows::Data::Json;
    auto service=Scheduler(); auto folder=Folder(service.get()); auto task=Existing(folder.get());
    JsonObject json; json.SetNamedValue(L"enabled",JsonValue::CreateBooleanValue(product::Enabled()!=0));
    json.SetNamedValue(L"supported",JsonValue::CreateBooleanValue(product::Compatible()));
    json.SetNamedValue(L"taskRegistered",JsonValue::CreateBooleanValue(task && Owned(task.get())));
    json.SetNamedValue(L"runtimePath",JsonValue::CreateStringValue(product::Runtime().wstring()));
    JsonArray active; for (auto pid : product::Hosts()) if (product::Active(pid)) active.Append(JsonValue::CreateNumberValue(pid));
    json.SetNamedValue(L"activeShellPids",active); product::Output(std::wstring(json.Stringify()));
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int) {
    bool commandLine=false;
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        int argc=0; auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        std::wstring command=argc>1?argv[1]:L""; LocalFree(argv); commandLine=!command.empty();
        if (command==L"--status") { Status(); return 0; }
        auto mutex=CreateMutexW(nullptr,TRUE,L"Local\\QuickSettingsThemeTile.Setup"); if (!mutex) winrt::throw_last_error();
        if (GetLastError()==ERROR_ALREADY_EXISTS) { CloseHandle(mutex); winrt::throw_hresult(HRESULT_FROM_WIN32(ERROR_BUSY)); }
        struct Guard { HANDLE value; ~Guard() { ReleaseMutex(value); CloseHandle(value); } } guard{mutex};
        if (command==L"--enable") { Enable(instance); return 0; }
        if (command==L"--uninstall") { Uninstall(); return 0; }
        if (!command.empty()) winrt::throw_hresult(E_INVALIDARG);
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES}; InitCommonControlsEx(&controls);
        TASKDIALOG_BUTTON buttons[]={{101,L"启用功能"},{102,L"卸载功能"}};
        TASKDIALOGCONFIG dialog{sizeof(dialog)}; dialog.hInstance=instance;
        dialog.dwFlags=TDF_ALLOW_DIALOG_CANCELLATION|TDF_SIZE_TO_CONTENT; dialog.dwCommonButtons=TDCBF_CANCEL_BUTTON;
        dialog.pszWindowTitle=L"快速设置主题磁贴"; dialog.pszMainInstruction=product::Enabled()?L"主题磁贴已启用":L"添加浅色 / 深色主题磁贴";
        dialog.pszContent=L"磁贴支持原生拖动。启用后可以关闭此工具，无需常驻安装工具或自建服务。\n\n当前版本仅适配 Windows 11 26300.9457。";
        dialog.cButtons=ARRAYSIZE(buttons); dialog.pButtons=buttons; dialog.nDefaultButton=product::Enabled()?IDCANCEL:101;
        int selected=0; check_hresult(TaskDialogIndirect(&dialog,&selected,nullptr,nullptr));
        if (selected==101) {
            bool active=Enable(instance);
            MessageBoxW(nullptr,active?L"已启用。你可以关闭此工具，然后打开快速设置。":L"已配置自动加载，但尚未确认 Shell 内的接入。\n\n打开快速设置后请等待最多约一分钟，再查看磁贴是否出现。",L"快速设置主题磁贴",MB_OK|MB_ICONINFORMATION);
        }
        else if (selected==102) {
            bool pending=Uninstall();
            MessageBoxW(nullptr,pending?L"功能和自动加载配置已卸载。Shell 内已停止运行。\n\n当前宿主仍映射着组件文件；它正常退出后，重新打开此工具并选择卸载即可清理剩余文件。":L"功能、自动加载配置与组件文件已卸载。",L"快速设置主题磁贴",MB_OK|MB_ICONINFORMATION);
        }
        return 0;
    } catch (winrt::hresult_error const& error) {
        auto message=L"操作失败，错误 "+std::to_wstring(static_cast<unsigned long>(error.code()))+L"。\n"+std::wstring(error.message());
        product::Output(L"{\"error\":"+std::to_wstring(static_cast<unsigned long>(error.code()))+L"}");
        if (!commandLine) MessageBoxW(nullptr,message.c_str(),L"快速设置主题磁贴",MB_OK|MB_ICONERROR); return 1;
    } catch (...) { if (!commandLine) MessageBoxW(nullptr,L"操作未完成。",L"快速设置主题磁贴",MB_OK|MB_ICONERROR); return 1; }
}
