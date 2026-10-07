#pragma once
#include "theme-backend.h"
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <memory>
#include <atomic>
#include <string>
#include <mutex>
#include <vector>
#include <functional>
#include <winrt/Windows.UI.Xaml.Input.h>

namespace themeui {
namespace wx=winrt::Windows::UI::Xaml;
namespace wp=winrt::Windows::UI::Xaml::Controls::Primitives;
namespace wa=winrt::Windows::UI::Xaml::Automation;
namespace wc=winrt::Windows::UI::Xaml::Controls;
using Logger=void(*)(std::wstring const&);
using NativeSetter=std::function<void(wc::Control const&,wchar_t const*,winrt::Windows::Foundation::IInspectable const&)>;
struct ThemeCommand : winrt::implements<ThemeCommand,winrt::Windows::UI::Xaml::Input::ICommand> {
    std::function<void()> execute;
    explicit ThemeCommand(std::function<void()> callback) : execute(std::move(callback)) {}
    bool CanExecute(winrt::Windows::Foundation::IInspectable const&) { return true; }
    void Execute(winrt::Windows::Foundation::IInspectable const&) { execute(); }
    winrt::event_token CanExecuteChanged(winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable> const& handler) { return changed.add(handler); }
    void CanExecuteChanged(winrt::event_token token) noexcept { changed.remove(token); }
    winrt::event<winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>> changed;
};

// Experimental controller. All writes and notifications run on one worker.
// Stop must run off the XAML thread, before removing the native model/template.
class Runtime : public std::enable_shared_from_this<Runtime> {
    struct Binding { winrt::weak_ref<wc::Control> weak; winrt::event_token click{}; NativeSetter nativeSetter; };
    struct DragBinding { winrt::weak_ref<wc::ListViewBase> weak; winrt::event_token starting,completed; };
    std::mutex mutex;
    std::vector<Binding> bindings;
    std::vector<DragBinding> dragBindings;
    std::atomic<bool> stopping{false},busy{false},dragging{false};
    std::atomic<ULONGLONG> lastDrag{0};
    HANDLE stopEvent=nullptr,requestEvent=nullptr,thread=nullptr;
    Logger log;
    static DWORD WINAPI WorkerEntry(void* context) noexcept {
        std::unique_ptr<std::shared_ptr<Runtime>> owner(static_cast<std::shared_ptr<Runtime>*>(context));
        (*owner)->Worker(); return 0;
    }
    void Paint(theme::State state,LSTATUS error,bool contrast,bool isBusy,std::wstring message) noexcept {
        std::vector<Binding> snapshot;
        { std::lock_guard guard(mutex); snapshot=bindings; }
        auto weakSelf=weak_from_this();
        for (auto const& binding : snapshot) {
            try {
                if (auto button=binding.weak.get()) {
                    button.Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                        [weakSelf,weak=binding.weak,nativeSetter=binding.nativeSetter,state,error,contrast,isBusy,message] {
                        auto self=weakSelf.lock();
                        if (!self || self->stopping) return;
                        if (auto target=weak.get()) {
                            try {
                                target.IsEnabled(!error && !contrast && !self->busy);
                                if (nativeSetter) nativeSetter(target,L"IsChecked",winrt::box_value(!error && state.dark()));
                                else if (auto toggle=target.try_as<wp::ToggleButton>()) {
                                    if (error || state.mixed()) toggle.IsChecked(nullptr);
                                    else toggle.IsChecked(state.dark());
                                }
                                std::wstring help=message;
                                if (help.empty()) {
                                    if (contrast) help=L"正在使用对比度主题";
                                    else if (error) help=L"无法读取主题状态，错误 "+std::to_wstring(error);
                                    else if (isBusy) help=L"正在切换主题";
                                    else if (state.mixed()) help=L"系统与应用模式不同；点击统一为深色";
                                    else help=state.dark()?L"切换为浅色模式":L"切换为深色模式";
                                }
                                wc::ToolTipService::SetToolTip(target,winrt::box_value(winrt::hstring{help}));
                                wa::AutomationProperties::SetName(target,L"深色模式");
                                wa::AutomationProperties::SetHelpText(target,winrt::hstring{help});
                            } catch (...) {}
                        }
                    });
                }
            } catch (...) {}
        }
    }
    void Refresh(std::wstring const& message=L"") noexcept {
        theme::State state;
        auto error=theme::Read(state);
        Paint(state,error,theme::HighContrast(),busy,message);
    }
    void Worker() noexcept {
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            theme::Key key;
            HANDLE changed=CreateEventW(nullptr,TRUE,FALSE,nullptr);
            bool armed=false;
            theme::State last{2,2}; LSTATUS lastError=-1; bool lastContrast=false,lastBusy=false;
            while (!stopping) {
                if (!key.value) RegOpenKeyExW(HKEY_CURRENT_USER,theme::KeyPath,0,KEY_NOTIFY,&key.value);
                if (key.value && changed && !armed) {
                    ResetEvent(changed);
                    armed=RegNotifyChangeKeyValue(key.value,FALSE,REG_NOTIFY_CHANGE_LAST_SET,changed,TRUE)==ERROR_SUCCESS;
                }
                HANDLE events[]={stopEvent,requestEvent,changed};
                auto wake=WaitForMultipleObjects(changed?3:2,events,FALSE,1000);
                if (wake==WAIT_OBJECT_0 || stopping) break;
                if (wake==WAIT_OBJECT_0+1) {
                    auto result=theme::Toggle();
                    busy=false;
                    std::wstring message;
                    if (result.error) message=L"切换失败，错误 "+std::to_wstring(result.error)+(result.rollbackFailed?L"；部分设置未能恢复":L"");
                    else if (result.notificationsPartial) message=L"主题已切换；部分应用需要重新打开";
                    log(L"THEME RESULT error="+std::to_wstring(result.error)+L" written="+std::to_wstring(result.written)+L" actualKnown="+std::to_wstring(result.actualKnown)+L" system="+std::to_wstring(result.actual.system)+L" apps="+std::to_wstring(result.actual.apps)+L" rollbackFailed="+std::to_wstring(result.rollbackFailed)+L" notificationsPartial="+std::to_wstring(result.notificationsPartial));
                    Refresh(message);
                    // Keep the completion message until a later external change.
                    lastError=theme::Read(last); lastContrast=theme::HighContrast(); lastBusy=false;
                    continue;
                }
                if (wake==WAIT_OBJECT_0+2) armed=false;
                if (wake==WAIT_FAILED) { log(L"THEME OBSERVER WAIT FAILED"); break; }
                // Skip intermediate two-value states while our own transaction is running.
                if (!busy) {
                    theme::State state;
                    auto error=theme::Read(state); bool contrast=theme::HighContrast(); bool isBusy=busy;
                    if (state.system!=last.system || state.apps!=last.apps || error!=lastError || contrast!=lastContrast || isBusy!=lastBusy) {
                        Paint(state,error,contrast,isBusy,L"");
                        last=state; lastError=error; lastContrast=contrast; lastBusy=isBusy;
                    }
                }
            }
            if (changed) CloseHandle(changed);
        } catch (...) { busy=false; log(L"THEME WORKER ERROR"); }
    }
public:
    explicit Runtime(Logger logger) : log(logger) {}
    ~Runtime() {
        if (thread) CloseHandle(thread);
        if (requestEvent) CloseHandle(requestEvent);
        if (stopEvent) CloseHandle(stopEvent);
    }
    bool Start() {
        stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        requestEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if (!stopEvent || !requestEvent) return false;
        auto owner=std::make_unique<std::shared_ptr<Runtime>>(shared_from_this());
        thread=CreateThread(nullptr,0,WorkerEntry,owner.get(),0,nullptr);
        if (!thread) return false;
        owner.release(); return true;
    }
    void Request() {
        if (dragging || (lastDrag && GetTickCount64()-lastDrag<350)) { Refresh(); log(L"THEME REQUEST IGNORED: drag guard"); return; }
        if (stopping || busy.exchange(true)) return;
        Refresh();
        if (!SetEvent(requestEvent)) { busy=false; Refresh(L"无法启动切换任务"); }
    }
    void Bind(wc::Control const& button,NativeSetter nativeSetter={}) {
        if (stopping) return;
        { std::lock_guard guard(mutex);
          std::erase_if(bindings,[](auto const& binding) { return !binding.weak.get(); });
          for (auto const& binding : bindings) if (binding.weak.get()==button) return; }
        auto weakSelf=weak_from_this();
        winrt::event_token token{};
        if (nativeSetter) {
            auto command=winrt::make<ThemeCommand>([weakSelf] { if (auto self=weakSelf.lock()) self->Request(); });
            nativeSetter(button,L"Command",command);
        } else {
            auto toggle=button.as<wp::ToggleButton>();
            toggle.IsThreeState(true);
            token=toggle.Click([weakSelf](auto const&,wx::RoutedEventArgs const&) { if (auto self=weakSelf.lock()) self->Request(); });
        }
        { std::lock_guard guard(mutex); bindings.push_back({winrt::make_weak(button),token,std::move(nativeSetter)}); }
        Refresh(); log(L"THEME BUTTON BOUND");
    }
    void BindDrag(wc::ListViewBase const& list,std::function<void(wc::ListViewBase const&)> orderChanged={}) {
        if (stopping) return;
        { std::lock_guard guard(mutex);
          std::erase_if(dragBindings,[](auto const& binding) { return !binding.weak.get(); });
          for (auto const& binding : dragBindings) if (binding.weak.get()==list) return; }
        auto weakSelf=weak_from_this();
        auto starting=list.DragItemsStarting([weakSelf](auto const&,auto const&) {
            if (auto self=weakSelf.lock()) { self->dragging=true; self->log(L"NATIVE DRAG STARTED"); }
        });
        auto completed=list.DragItemsCompleted([weakSelf,orderChanged](auto const& sender,auto const&) {
            if (auto self=weakSelf.lock()) {
                self->lastDrag=GetTickCount64(); self->dragging=false; self->log(L"NATIVE DRAG COMPLETED");
                if (orderChanged) sender.Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,[weakSelf,weak=winrt::make_weak(sender),orderChanged] {
                    if (auto owner=weakSelf.lock(); owner && !owner->stopping) {
                        try { if (auto current=weak.get()) orderChanged(current); } catch (...) { owner->log(L"POSITION SAVE AFTER DRAG ERROR"); }
                    }
                });
            }
        });
        std::lock_guard guard(mutex); dragBindings.push_back({winrt::make_weak(list),starting,completed});
    }
    void Stop() noexcept {
        stopping=true;
        if (stopEvent) SetEvent(stopEvent);
        if (thread) WaitForSingleObject(thread,INFINITE); // bounded notifications; never called on UI thread
        std::vector<Binding> snapshot;
        { std::lock_guard guard(mutex); snapshot.swap(bindings); }
        for (auto const& binding : snapshot) {
            try { if (auto button=binding.weak.get()) {
                auto action=button.Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,[binding] {
                    try { if (auto target=binding.weak.get()) {
                        if (binding.nativeSetter) binding.nativeSetter(target,L"Command",nullptr);
                        else target.as<wp::ToggleButton>().Click(binding.click);
                        target.IsEnabled(false);
                    } } catch (...) {}
                });
                action.get();
            } } catch (...) {}
        }
        std::vector<DragBinding> dragSnapshot;
        { std::lock_guard guard(mutex); dragSnapshot.swap(dragBindings); }
        for (auto const& binding : dragSnapshot) {
            try { if (auto list=binding.weak.get()) {
                auto action=list.Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,[binding] {
                    try { if (auto target=binding.weak.get()) { target.DragItemsStarting(binding.starting); target.DragItemsCompleted(binding.completed); } } catch (...) {}
                }); action.get();
            } } catch (...) {}
        }
        log(L"THEME CONTROLLER STOPPED");
    }
};
}
