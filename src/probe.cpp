// Diagnostic research prototype. Default build reads structure. QS_NATIVE_ACTION_PROTOTYPE
// additionally registers a native view-model with a module-owned template for the draggable tile
// experiment. QS_THEME_FUNCTIONAL separately enables the theme controller.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <xamlom.h>
#include <ocidl.h>
#undef GetCurrentTime
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Data.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include "probe-id.h"
#include "native-abi.h"
#ifdef QS_PRODUCT
#include "product-common.h"
#endif
#include "tile-order.h"
#ifdef QS_THEME_FUNCTIONAL
#include "theme-runtime.h"
#endif

namespace wf = winrt::Windows::Foundation;
namespace wx = winrt::Windows::UI::Xaml;
namespace wc = winrt::Windows::UI::Xaml::Controls;
namespace wi = winrt::Windows::UI::Xaml::Interop;
namespace wd = winrt::Windows::UI::Xaml::Data;
namespace wm = winrt::Windows::UI::Xaml::Markup;

// Undocumented diagnostic cleanup interface, identified by Notification Center Styler (GPL-3.0).
struct __declspec(uuid("735941a2-3ee3-495a-8da9-972627003075")) DiagnosticsCleanup : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE UnregisterInstance(InstanceHandle handle) = 0;
    virtual HRESULT STDMETHODCALLTYPE TryGetDispatcherQueueForObject(InstanceHandle handle,void** queue) = 0;
};

namespace {
std::mutex logMutex;
HANDLE logFile = INVALID_HANDLE_VALUE;
std::atomic<bool> running{false};
std::atomic<bool> prototypeAttempted{false};
constexpr wchar_t ThemeTemplateKey[]=L"CodexThemeTogglePrototypeTemplateV6";

void Log(std::wstring const& value) noexcept {
    try {
#ifdef QS_PRODUCT
        // Installed runs keep short operational logs instead of a complete visual-tree trace.
        bool operational=value.find(L"ERROR")!=std::wstring::npos;
        for (auto prefix : {L"THEME",L"PROBE",L"PRODUCT",L"ADVISE",L"UNADVISE",L"SESSION",L"POSITION",L"PROTOTYPE",L"NATIVE STYLE",L"NATIVE DRAG"}) if (value.starts_with(prefix)) operational=true;
        if (!operational) return;
#endif
        auto text = winrt::to_string(value + L"\r\n");
        std::lock_guard guard(logMutex);
        if (logFile != INVALID_HANDLE_VALUE) { DWORD written; WriteFile(logFile,text.data(),static_cast<DWORD>(text.size()),&written,nullptr); }
    } catch (...) {}
}

std::wstring Class(wf::IInspectable const& object) {
    if (!object) return L"(null)";
    try { return std::wstring(winrt::get_class_name(object)); } catch (...) { return L"(class unavailable)"; }
}

void Iids(std::wstring const& prefix, wf::IInspectable const& object) {
    if (!object) return;
    auto abi = reinterpret_cast<::IInspectable*>(winrt::get_abi(object));
    ULONG count = 0; IID* ids = nullptr;
    if (SUCCEEDED(abi->GetIids(&count,&ids))) {
        for (ULONG i = 0; i < count; ++i) { wchar_t id[40]; StringFromGUID2(ids[i],id,40); Log(prefix + L" IID " + id); }
        CoTaskMemFree(ids);
    }
}

std::wstring Value(wf::IInspectable const& object) {
    if (!object) return L"(null)";
    if (auto value = object.try_as<wf::IPropertyValue>()) {
        switch (value.Type()) {
        case wf::PropertyType::String: return L"string:" + std::wstring(value.GetString());
        case wf::PropertyType::Boolean: return value.GetBoolean() ? L"bool:1" : L"bool:0";
        case wf::PropertyType::Int32: return L"i32:" + std::to_wstring(value.GetInt32());
        case wf::PropertyType::UInt32: return L"u32:" + std::to_wstring(value.GetUInt32());
        case wf::PropertyType::Guid: { wchar_t id[40]; auto guid = value.GetGuid(); StringFromGUID2(guid,id,40); return L"guid:" + std::wstring(id); }
        default: break;
        }
    }
    return L"class:" + Class(object);
}

wm::IXamlMetadataProvider Metadata() {
    thread_local wm::IXamlMetadataProvider metadata{nullptr};
    if (!metadata) {
                auto module = GetModuleHandleW(L"ControlCenter.dll");
                using FactoryFn = HRESULT (WINAPI*)(HSTRING,void**);
                auto activate = module ? reinterpret_cast<FactoryFn>(GetProcAddress(module,"DllGetActivationFactory")) : nullptr;
                if (activate) {
                    wf::IActivationFactory factory{nullptr};
                    winrt::hstring typeName{L"ControlCenter.XamlMetaDataProvider"};
                    auto hr=activate(static_cast<HSTRING>(winrt::get_abi(typeName)),winrt::put_abi(factory));
                    if (SUCCEEDED(hr)) metadata=factory.ActivateInstance<wf::IInspectable>().try_as<wm::IXamlMetadataProvider>();
                    else Log(L"METADATA FACTORY HRESULT="+std::to_wstring(static_cast<unsigned long>(hr)));
                }
    }
    return metadata;
}

wf::IInspectable Property(wf::IInspectable const& object, wchar_t const* name, std::wstring const& prefix,bool trace=true) {
    try {
        if (object && Class(object).starts_with(L"ControlCenter.")) {
            auto metadata=Metadata();
            if (metadata) {
                auto type=metadata.GetXamlType(winrt::hstring{Class(object)});
                if (type) {
                    auto member=type.GetMember(name);
                    if (member) {
                        auto value=member.GetValue(object);
                        if (trace) Log(prefix+L"."+name+L" = "+Value(value));
                        return value;
                    }
                }
            }
        }
        if (auto provider = object.try_as<wd::ICustomPropertyProvider>()) {
            if (auto property = provider.GetCustomProperty(name)) {
                auto value = property.GetValue(object);
                if (trace) Log(prefix + L"." + name + L" = " + Value(value));
                return value;
            }
        }
    } catch (...) { Log(prefix + L"." + name + L" ERROR " + std::to_wstring(static_cast<unsigned long>(winrt::to_hresult()))); }
    return nullptr;
}

void Model(std::wstring const& prefix, wf::IInspectable const& object) {
    Log(prefix + L" class=" + Class(object)); Iids(prefix,object);
    // Backend getters such as Id can assume the underlying action is initialized, and fault
    // for hidden/inapplicable layout models. Read only locally stored fields for arbitrary models.
    for (auto name : { L"FriendlyName",L"CustomTemplateKey",L"ContainingGroupName" }) Property(object,name,prefix);
}

void Collection(std::wstring const& prefix, wf::IInspectable const& object) {
    Log(prefix + L" class=" + Class(object)); Iids(prefix,object);
    if (!object) return;
    if (auto vector = object.try_as<wi::IBindableVector>()) {
        auto count = vector.Size(); Log(prefix + L" size=" + std::to_wstring(count));
        for (uint32_t i = 0; i < count && i < 64; ++i) Model(prefix+L"["+std::to_wstring(i)+L"]",vector.GetAt(i));
    } else if (auto typedVector = object.try_as<wf::Collections::IVector<wf::IInspectable>>()) {
        auto count = typedVector.Size(); Log(prefix + L" size=" + std::to_wstring(count));
        for (uint32_t i = 0; i < count && i < 64; ++i) Model(prefix+L"["+std::to_wstring(i)+L"]",typedVector.GetAt(i));
    } else if (auto view = object.try_as<wi::IBindableVectorView>()) {
        auto count = view.Size(); Log(prefix + L" size=" + std::to_wstring(count));
        for (uint32_t i = 0; i < count && i < 64; ++i) Model(prefix+L"["+std::to_wstring(i)+L"]",view.GetAt(i));
    }
}

void Geometry(wx::FrameworkElement const& element) {
    std::wstring snapshot;
    auto capture=[&](std::wstring const& line) { snapshot+=line+L"\r\n"; };
    auto prefix=Class(element)+L"#"+std::wstring(element.Name());
    auto bounds=element.TransformToVisual(nullptr).TransformBounds({0,0,static_cast<float>(element.ActualWidth()),static_cast<float>(element.ActualHeight())});
    auto xamlRoot=element.XamlRoot();
    auto hostVisible=xamlRoot ? (xamlRoot.IsHostVisible()?L"1":L"0") : L"unknown";
    capture(L"GEOMETRY "+prefix+L" loaded="+std::to_wstring(element.IsLoaded())+L" visible="+std::to_wstring(element.Visibility()==wx::Visibility::Visible)+L" hostVisible="+hostVisible+L" opacity="+std::to_wstring(element.Opacity())+L" actual="+std::to_wstring(element.ActualWidth())+L","+std::to_wstring(element.ActualHeight())+L" rootBounds="+std::to_wstring(bounds.X)+L","+std::to_wstring(bounds.Y)+L","+std::to_wstring(bounds.Width)+L","+std::to_wstring(bounds.Height));
    wx::DependencyObject parent=element;
    for (unsigned depth=0;depth<32 && parent;++depth) {
        if (auto fe=parent.try_as<wx::FrameworkElement>()) {
            capture(L"ANCESTOR "+prefix+L" depth="+std::to_wstring(depth)+L" class="+Class(fe)+L" name="+std::wstring(fe.Name())+L" visibility="+std::to_wstring(static_cast<int>(fe.Visibility()))+L" opacity="+std::to_wstring(fe.Opacity())+L" actual="+std::to_wstring(fe.ActualWidth())+L","+std::to_wstring(fe.ActualHeight()));
        }
        if (auto scroll=parent.try_as<wc::ScrollViewer>()) {
            auto viewport=scroll.TransformToVisual(nullptr).TransformBounds({0,0,static_cast<float>(scroll.ActualWidth()),static_cast<float>(scroll.ActualHeight())});
            bool intersects=bounds.X<viewport.X+viewport.Width && bounds.X+bounds.Width>viewport.X && bounds.Y<viewport.Y+viewport.Height && bounds.Y+bounds.Height>viewport.Y;
            capture(L"SCROLL "+prefix+L" offset="+std::to_wstring(scroll.HorizontalOffset())+L","+std::to_wstring(scroll.VerticalOffset())+L" viewport="+std::to_wstring(scroll.ViewportWidth())+L","+std::to_wstring(scroll.ViewportHeight())+L" intersects="+std::to_wstring(intersects));
        }
        parent=wx::Media::VisualTreeHelper::GetParent(parent);
    }
    // A continuous session records geometry changes rather than repeating a full tree every 5s.
    thread_local std::unordered_map<void*,std::wstring> previous;
    if (previous.size()>256) previous.clear();
    auto identity=winrt::get_abi(element);
    if (previous[identity]!=snapshot) { previous[identity]=snapshot; Log(snapshot); }
}

void Inspect(wx::FrameworkElement const& element) {
    auto prefix = Class(element) + L"#" + std::wstring(element.Name());
    Log(L"INSPECT " + prefix + L" UI-thread="+std::to_wstring(GetCurrentThreadId()));
    Iids(prefix,element);
    auto context = element.DataContext();
    Model(prefix+L".DataContext",context);
    auto group = Property(element,L"Group",prefix);
    if (!group) group = context;
    if (group) {
        Iids(prefix+L".Group",group);
        for (auto name : { L"DisplayedQuickActions",L"UserQuickActions",L"DefaultQuickActions",L"QuickActions" }) {
            auto vector = Property(group,name,prefix+L".Group");
            if (vector) Collection(prefix+L".Group."+name,vector);
        }
    }
    if (auto list = element.try_as<wc::ListViewBase>()) {
        Log(prefix + L" CanReorder="+std::to_wstring(list.CanReorderItems())+L" CanDrag="+std::to_wstring(list.CanDragItems())+L" AllowDrop="+std::to_wstring(list.AllowDrop()));
        Collection(prefix+L".ItemsSource",list.ItemsSource());
        Log(prefix+L" Items.Count="+std::to_wstring(list.Items().Size()));
        Log(prefix+L" TemplateSelector="+Class(list.ItemTemplateSelector()));
    }
}

#ifdef QS_NATIVE_ACTION_PROTOTYPE
#if defined(QS_THEME_FUNCTIONAL) || defined(QS_CLEANUP_NATIVE)
void RemoveNativeAction(wx::FrameworkElement const& element) {
    auto list=element.try_as<wc::ListViewBase>();
    if (!list || element.Name()!=L"RootGridView" || !list.CanReorderItems()) return;
    auto group=element.DataContext();
    for (auto name : {L"DisplayedQuickActions",L"UserQuickActions",L"DefaultQuickActions"}) {
        auto vector=Property(group,name,L"CLEANUP GROUP").try_as<wf::Collections::IVector<wf::IInspectable>>();
        if (!vector) continue;
        for (uint32_t i=vector.Size();i>0;--i) {
            auto value=Property(vector.GetAt(i-1),L"FriendlyName",L"CLEANUP LOOKUP");
            if (value && winrt::unbox_value<winrt::hstring>(value)==L"Codex.QuickAction.ThemeToggle") vector.RemoveAt(i-1);
        }
    }
    auto resources=wx::Application::Current().Resources();
    for (auto name : {L"CodexThemeTogglePrototypeTemplate",L"CodexThemeTogglePrototypeTemplateV2",L"CodexThemeTogglePrototypeTemplateV3",L"CodexThemeTogglePrototypeTemplateV4",L"CodexThemeTogglePrototypeTemplateV5",ThemeTemplateKey,L"CodexThemeNativeTileStyle",L"CodexThemeNativeTileControlTemplate"}) {
        auto key=winrt::box_value(winrt::hstring{name});
        if (resources.HasKey(key)) resources.Remove(key);
    }
    Log(L"PROTOTYPE REMOVED");
}
#endif
#ifndef QS_CLEANUP_NATIVE
std::vector<std::wstring> ActionNames(wf::Collections::IVector<wf::IInspectable> const& vector) {
    std::vector<std::wstring> result;
    for (uint32_t i=0;i<vector.Size();++i) {
        auto name=Property(vector.GetAt(i),L"FriendlyName",L"ORDER LOOKUP",false);
        result.push_back(name?std::wstring(winrt::unbox_value<winrt::hstring>(name)):L"");
    }
    return result;
}
bool TileMetrics(wx::DependencyObject const& root,double containerWidth,double& width,double& height,wc::Control& sample,unsigned depth=0) {
    if (!root || depth>20) return false;
    if (auto fe=root.try_as<wx::FrameworkElement>(); fe && fe.Name()!=L"CodexThemePrototypeButton" && Class(fe)==L"ControlCenter.PaginatedToggleButton" && fe.ActualWidth()>=containerWidth*0.8 && fe.ActualHeight()>0) {
        width=fe.ActualWidth(); height=fe.ActualHeight(); sample=fe.as<wc::Control>(); return true;
    }
    auto count=wx::Media::VisualTreeHelper::GetChildrenCount(root);
    for (int i=0;i<count;++i) if (TileMetrics(wx::Media::VisualTreeHelper::GetChild(root,i),containerWidth,width,height,sample,depth+1)) return true;
    return false;
}
void InstallNativeAction(wx::FrameworkElement const& element) {
    auto list=element.try_as<wc::ListViewBase>();
    if (!list || element.Name()!=L"RootGridView" || !list.CanReorderItems()) return;
#ifndef QS_RESIDENT_NATIVE
    if (prototypeAttempted.exchange(true)) return;
#endif
    auto module=GetModuleHandleW(L"ControlCenter.dll");
    if (!module) { Log(L"PROTOTYPE REFUSED: no ControlCenter module"); return; }
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(module);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(reinterpret_cast<std::byte*>(module)+dos->e_lfanew);
    auto profile=native_abi::ProfileForImage(nt->FileHeader.Machine,nt->FileHeader.TimeDateStamp,nt->OptionalHeader.SizeOfImage);
    if (!profile) {
        Log(L"PROTOTYPE REFUSED: unverified ControlCenter image"); return;
    }
    auto initializeRva=profile->initializeRva;
    auto group=element.DataContext();
    auto groupName=Property(group,L"Name",L"PROTOTYPE GROUP",false);
    if (!groupName || winrt::unbox_value<winrt::hstring>(groupName)!=L"Toggles") { Log(L"PROTOTYPE REFUSED: wrong group"); return; }
    // Use the component's XAML schema to retrieve its collections instead of duplicating a private group ABI.
    auto defaultSource=Property(group,L"DefaultQuickActions",L"PROTOTYPE GROUP",false);
    auto userSource=Property(group,L"UserQuickActions",L"PROTOTYPE GROUP",false);
    auto defaults=defaultSource.try_as<wf::Collections::IVector<wf::IInspectable>>();
    auto user=userSource.try_as<wf::Collections::IVector<wf::IInspectable>>();
    auto displayed=Property(group,L"DisplayedQuickActions",L"PROTOTYPE GROUP",false).try_as<wf::Collections::IVector<wf::IInspectable>>();
    if (!defaults || !user || !displayed) { Log(L"PROTOTYPE REFUSED: native vector bridge unavailable"); return; }
#ifdef QS_RESIDENT_NATIVE
    // Recheck the current group, rather than keeping a process-wide 'installed' flag.
    // Shell can reconstruct its panel without changing process id.
    for (uint32_t i=0;i<displayed.Size();++i) {
        auto name=Property(displayed.GetAt(i),L"FriendlyName",L"RESIDENT LOOKUP",false);
        if (name && winrt::unbox_value<winrt::hstring>(name)==L"Codex.QuickAction.ThemeToggle") {
            auto templateKey=Property(displayed.GetAt(i),L"CustomTemplateKey",L"RESIDENT TEMPLATE",false);
            if (templateKey && winrt::unbox_value<winrt::hstring>(templateKey)==ThemeTemplateKey) {
                auto error=tileorder::Save(ActionNames(displayed));
                if (error) Log(L"POSITION SAVE ERROR="+std::to_wstring(error));
                return;
            }
        }
    }
#endif
    // Prepare the new model/template before touching the visible incarnation. Preserve each
    // collection's own position, including the displayed order chosen by a native drag.
    struct Previous { wf::Collections::IVector<wf::IInspectable> vector;
        std::vector<std::pair<uint32_t,wf::IInspectable>> entries; };
    std::vector<Previous> previous{{displayed,{}},{user,{}},{defaults,{}}};
    for (auto& snapshot : previous) {
        for (uint32_t i=0;i<snapshot.vector.Size();++i) {
            auto value=snapshot.vector.GetAt(i);
            auto name=Property(value,L"FriendlyName",L"PREVIOUS PROTOTYPE LOOKUP",false);
            if (name && winrt::unbox_value<winrt::hstring>(name)==L"Codex.QuickAction.ThemeToggle") snapshot.entries.emplace_back(i,value);
        }
    }
    auto metadata=Metadata();
    auto type=metadata.GetXamlType(L"ControlCenter.QuickActionViewModel");
    if (!type || !type.IsConstructible()) { Log(L"PROTOTYPE REFUSED: native model factory unavailable"); return; }
    auto model=type.ActivateInstance();
    constexpr wchar_t actionName[]=L"Codex.QuickAction.ThemeToggle";
    auto templateName=ThemeTemplateKey;
    for (auto const& entry : { std::pair{L"FriendlyName",actionName}, std::pair{L"ContainingGroupName",L"Toggles"}, std::pair{L"CustomTemplateKey",templateName},std::pair{L"QuickActionTitle",L"深色模式"} }) {
        auto member=type.GetMember(entry.first);
        if (!member || member.IsReadOnly()) { Log(L"PROTOTYPE REFUSED: required member unavailable"); return; }
        member.SetValue(model,winrt::box_value(winrt::hstring{entry.second}));
    }
    auto handler=type.GetMember(L"IsHandlerRequired");
    if (!handler || handler.IsReadOnly()) { Log(L"PROTOTYPE REFUSED: handler flag member unavailable"); return; }
    handler.SetValue(model,winrt::box_value(false));
    auto modelAbi=reinterpret_cast<::IInspectable*>(winrt::get_abi(model));
    NativeModelInitialize* initialize=nullptr;
    auto initializeQuery=modelAbi->QueryInterface(profile->initializeIid,reinterpret_cast<void**>(&initialize));
    if (FAILED(initializeQuery) || !initialize) { Log(L"PROTOTYPE REFUSED: profile Initialize IID query failed="+compatibility::HexValue(static_cast<uint32_t>(initializeQuery))); return; }
    auto initializeTable=*reinterpret_cast<void***>(initialize);
    if (initializeTable[6]!=reinterpret_cast<std::byte*>(module)+initializeRva) { initialize->Release(); Log(L"PROTOTYPE REFUSED: Initialize ABI fingerprint mismatch"); return; }
    auto initializeResult=initialize->Initialize();
    initialize->Release();
    winrt::check_hresult(initializeResult);
    Model(L"NATIVE THEME MODEL",model);
    auto applicable=Property(model,L"IsApplicable",L"NATIVE THEME MODEL");
    if (!applicable || !winrt::unbox_value<bool>(applicable)) { Log(L"PROTOTYPE REFUSED: custom native model is inapplicable"); return; }
    auto resources=wx::Application::Current().Resources();
    auto resourceKey=winrt::box_value(winrt::hstring{templateName});
    if (resources.HasKey(resourceKey)) resources.Remove(resourceKey);
    double tileWidth=0,tileHeight=0;
    wc::Control sample{nullptr};
    for (uint32_t i=0;i<list.Items().Size();++i) {
        if (auto container=list.ContainerFromIndex(i).try_as<wx::FrameworkElement>()) {
            if (TileMetrics(container,container.ActualWidth(),tileWidth,tileHeight,sample)) break;
        }
    }
    if (tileWidth<=0 || tileHeight<=0) { Log(L"PROTOTYPE REFUSED: native tile measurements unavailable"); return; }
    Log(L"NATIVE TILE METRICS width="+std::to_wstring(tileWidth)+L" height="+std::to_wstring(tileHeight));
    auto nativeStyle=sample.Style();
    auto nativeTemplate=sample.Template();
    Log(L"NATIVE STYLE present="+std::to_wstring(static_cast<bool>(nativeStyle))+L" template="+std::to_wstring(static_cast<bool>(nativeTemplate)));
    if (nativeStyle) {
        Log(L"NATIVE STYLE target="+std::wstring(nativeStyle.TargetType().Name));
        resources.Insert(winrt::box_value(winrt::hstring{L"CodexThemeNativeTileStyle"}),nativeStyle);
    }
    if (nativeTemplate) resources.Insert(winrt::box_value(winrt::hstring{L"CodexThemeNativeTileControlTemplate"}),nativeTemplate);
    if (!nativeTemplate) { Log(L"PROTOTYPE REFUSED: native button template unavailable"); return; }
    auto xaml=std::wstring{LR"xaml(
<DataTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" xmlns:cc="using:ControlCenter">
  <StackPanel Width="__WIDTH__" HorizontalAlignment="Center" VerticalAlignment="Top">
    <cc:PaginatedToggleButton x:Name="CodexThemePrototypeButton" __NATIVE_STYLE__ Template="{StaticResource CodexThemeNativeTileControlTemplate}" Width="__WIDTH__" Height="__HEIGHT__" HorizontalContentAlignment="Center" VerticalContentAlignment="Center" IsEnabled="False" ToolTipService.ToolTip="正在读取主题状态">
      <FontIcon Glyph="&#xE708;" FontFamily="{ThemeResource SymbolThemeFontFamily}" FontSize="16">
        <FontIcon.RenderTransform><TranslateTransform X="-1"/></FontIcon.RenderTransform>
      </FontIcon>
    </cc:PaginatedToggleButton>
    <TextBlock Text="深色模式" FontSize="12" Margin="0,8,0,0" HorizontalAlignment="Center" TextTrimming="CharacterEllipsis"/>
  </StackPanel>
</DataTemplate>)xaml"};
    auto replace=[&](std::wstring const& token,double number) {
        for (size_t position=0;(position=xaml.find(token,position))!=std::wstring::npos;) {
            auto value=std::to_wstring(number); xaml.replace(position,token.size(),value); position+=value.size();
        }
    };
    xaml.replace(xaml.find(L"__NATIVE_STYLE__"),std::wstring{L"__NATIVE_STYLE__"}.size(),nativeStyle?L"Style=\"{StaticResource CodexThemeNativeTileStyle}\"":L"");
    replace(L"__WIDTH__",tileWidth); replace(L"__HEIGHT__",tileHeight);
    auto dataTemplate=wm::XamlReader::Load(winrt::hstring{xaml}).as<wx::DataTemplate>();
    resources.Insert(resourceKey,dataTemplate);
    auto contains=[](wf::Collections::IVector<wf::IInspectable> const& vector) {
        for (uint32_t i=0;i<vector.Size();++i) {
            auto value=Property(vector.GetAt(i),L"FriendlyName",L"LOOKUP");
            if (value && winrt::unbox_value<winrt::hstring>(value)==L"Codex.QuickAction.ThemeToggle") return true;
        }
        return false;
    };
    auto before=list.Items().Size();
    try {
        for (auto const& snapshot : previous) {
            for (auto it=snapshot.entries.rbegin();it!=snapshot.entries.rend();++it) snapshot.vector.RemoveAt(it->first);
        }
        if (!contains(defaults)) defaults.InsertAt(previous[2].entries.empty()?defaults.Size():(std::min)(previous[2].entries.front().first,defaults.Size()),model);
        if (!contains(user)) {
            uint32_t insertion=previous[1].entries.empty()?user.Size():(std::min)(previous[1].entries.front().first,user.Size());
            if (previous[1].entries.empty()) {
                for (uint32_t i=0;i<user.Size();++i) {
                    auto name=Property(user.GetAt(i),L"FriendlyName",L"INSERTION ANCHOR",false);
                    if (name && winrt::unbox_value<winrt::hstring>(name)==L"Microsoft.QuickAction.LiveCaptions") { insertion=i; break; }
                }
                insertion=tileorder::Restore(ActionNames(user),insertion);
            }
            user.InsertAt(insertion,model);
        }
        auto displayedIndex=previous[0].entries.empty()?tileorder::Restore(ActionNames(displayed),5u):(std::min)(previous[0].entries.front().first,displayed.Size());
        if (!contains(displayed)) displayed.InsertAt(displayedIndex,model);
        Log(L"PROTOTYPE POSITION displayed="+std::to_wstring(displayedIndex)+L" preserved="+std::to_wstring(!previous[0].entries.empty()));
        auto saveError=tileorder::Save(ActionNames(displayed));
        Log(L"POSITION SAVE error="+std::to_wstring(saveError));
        Log(L"PROTOTYPE INSTALLED: native defaults="+std::to_wstring(defaults.Size())+L" user="+std::to_wstring(user.Size())+L" displayed before="+std::to_wstring(before)+L" after="+std::to_wstring(list.Items().Size()));
        Inspect(element);
    } catch (...) {
        uint32_t index=0;
        if (displayed.IndexOf(model,index)) displayed.RemoveAt(index);
        if (user.IndexOf(model,index)) user.RemoveAt(index);
        if (defaults.IndexOf(model,index)) defaults.RemoveAt(index);
        for (auto const& snapshot : previous) {
            for (auto const& entry : snapshot.entries) {
                if (!snapshot.vector.IndexOf(entry.second,index)) snapshot.vector.InsertAt((std::min)(entry.first,snapshot.vector.Size()),entry.second);
            }
        }
        resources.Remove(resourceKey);
        Log(L"PROTOTYPE ROLLED BACK"); throw;
    }
}
#endif
#endif

struct Watcher : winrt::implements<Watcher,IVisualTreeServiceCallback2> {
    winrt::com_ptr<IXamlDiagnostics> diagnostics;
    winrt::com_ptr<IVisualTreeService3> service;
    std::mutex handleMutex;
    std::unordered_set<InstanceHandle> handles;
    std::vector<InstanceHandle> pendingRelease;
    std::vector<winrt::weak_ref<wx::FrameworkElement>> candidates;
    std::vector<winrt::weak_ref<wx::FrameworkElement>> ownButtons;
#ifdef QS_THEME_FUNCTIONAL
    std::shared_ptr<themeui::Runtime> themeRuntime;
#endif
    Watcher(winrt::com_ptr<IXamlDiagnostics> const& value) : diagnostics(value),service(value.as<IVisualTreeService3>()) {}
    HRESULT STDMETHODCALLTYPE OnVisualTreeChange(ParentChildRelation relation, VisualElement element, VisualMutationType mutation) noexcept override {
        if (!running) return S_OK;
        try {
            if (mutation == Add) {
                { std::lock_guard guard(handleMutex); handles.insert(element.Handle); if (relation.Parent) handles.insert(relation.Parent); }
                std::wstring type = element.Type ? element.Type : L"";
                std::wstring name = element.Name ? element.Name : L"";
                // Structure only: do not record TextBlock content, network names, notifications or media.
                Log(L"ADD "+std::to_wstring(element.Handle)+L" parent="+std::to_wstring(relation.Parent)+L" type="+type+L" name="+name);
                if (name==L"CodexThemePrototypeButton") {
                    wf::IInspectable object{nullptr};
                    winrt::check_hresult(diagnostics->GetIInspectableFromHandle(element.Handle,reinterpret_cast<::IInspectable**>(winrt::put_abi(object))));
                    if (auto fe=object.try_as<wx::FrameworkElement>()) { std::lock_guard guard(handleMutex); ownButtons.push_back(winrt::make_weak(fe)); }
                }
#ifdef QS_THEME_FUNCTIONAL
                if (name==L"CodexThemePrototypeButton" && themeRuntime) {
                    wf::IInspectable object{nullptr};
                    winrt::check_hresult(diagnostics->GetIInspectableFromHandle(element.Handle,reinterpret_cast<::IInspectable**>(winrt::put_abi(object))));
                    if (auto button=object.try_as<wc::Control>()) {
                        auto runtime=themeRuntime;
                        auto bind=[runtime](wc::Control const& target) {
                            if (Class(target)==L"ControlCenter.PaginatedToggleButton") {
                                runtime->Bind(target,[](wc::Control const& control,wchar_t const* name,wf::IInspectable const& value) {
                                    auto type=Metadata().GetXamlType(L"ControlCenter.PaginatedToggleButton");
                                    auto member=type.GetMember(name);
                                    if (!member || member.IsReadOnly()) winrt::throw_hresult(E_NOTIMPL);
                                    member.SetValue(control,value);
                                });
                                Log(L"THEME NATIVE CONTROL: "+Class(target));
                            } else if (target.try_as<themeui::wp::ToggleButton>()) runtime->Bind(target);
                        };
                        if (button.Dispatcher().HasThreadAccess()) bind(button);
                        else button.Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,[bind,weak=winrt::make_weak(button)] {
                            try { if (auto target=weak.get()) bind(target); } catch (...) { Log(L"NATIVE BUTTON BIND ERROR"); }
                        });
                    }
                }
#endif
                if (type.find(L"PaginatedGridView") != std::wstring::npos || name == L"RootGridView") {
                    wf::IInspectable object{nullptr};
                    winrt::check_hresult(diagnostics->GetIInspectableFromHandle(element.Handle,reinterpret_cast<::IInspectable**>(winrt::put_abi(object))));
                    if (auto fe = object.try_as<wx::FrameworkElement>()) {
                        { std::lock_guard guard(handleMutex); candidates.push_back(winrt::make_weak(fe)); }
                        Inspect(fe);
                    }
                }
            } else {
                std::lock_guard guard(handleMutex);
                if (handles.erase(element.Handle)) pendingRelease.push_back(element.Handle);
            }
        } catch (...) { Log(L"CALLBACK ERROR "+std::to_wstring(static_cast<unsigned long>(winrt::to_hresult()))); }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnElementStateChanged(InstanceHandle,VisualElementState,LPCWSTR) noexcept override { return S_OK; }
};

DWORD WINAPI Session(void* context) noexcept {
    auto watcher = winrt::com_ptr<Watcher>{}; watcher.attach(static_cast<Watcher*>(context));
#ifdef QS_PRODUCT
    HANDLE activeEvent=nullptr;
    struct ActiveGuard { HANDLE& event; ~ActiveGuard() { if (event) { SetEvent(event); CloseHandle(event); } } } activeGuard{activeEvent};
#endif
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        auto cleanup = watcher->diagnostics.try_as<DiagnosticsCleanup>();
        if (!cleanup) {
#ifdef QS_THEME_FUNCTIONAL
            if (watcher->themeRuntime) watcher->themeRuntime->Stop();
#endif
            running=false;
            Log(L"SESSION SKIPPED: diagnostics cleanup interface unavailable");
            std::lock_guard guard(logMutex);
            if (logFile!=INVALID_HANDLE_VALUE) { CloseHandle(logFile); logFile=INVALID_HANDLE_VALUE; }
            return 1;
        }
        auto hr = watcher->service->AdviseVisualTreeChange(watcher.get());
        Log(L"ADVISE HRESULT="+std::to_wstring(static_cast<unsigned long>(hr)));
        if (SUCCEEDED(hr)) {
#ifdef QS_PRODUCT
            activeEvent=CreateEventW(nullptr,TRUE,FALSE,(std::wstring(product::ActivePrefix)+std::to_wstring(GetCurrentProcessId())).c_str());
            if (activeEvent) ResetEvent(activeEvent);
#endif
            // A bounded research session. No permanent event subscription or process hook.
            auto stopName=L"Local\\QuickSettingsThemeProbe.Stop."+std::to_wstring(GetCurrentProcessId());
            HANDLE stopEvent=CreateEventW(nullptr,TRUE,FALSE,stopName.c_str());
#ifdef QS_KEEP_ATTACHED
            Log(L"SESSION MODE=CONTINUOUS; no automatic timeout");
#elif defined(QS_THEME_FUNCTIONAL) || defined(QS_RESIDENT_NATIVE)
            constexpr unsigned iterations=180; // functional prototype: 15 minutes, or --stop
#else
            constexpr unsigned iterations=12;
#endif
            for (unsigned iteration=0;
#ifdef QS_KEEP_ATTACHED
                 true;
#else
                 iteration < iterations;
#endif
                 ++iteration) {
                if (stopEvent && WaitForSingleObject(stopEvent,5000)==WAIT_OBJECT_0) break;
                if (!stopEvent) Sleep(5000);
#ifdef QS_PRODUCT
                if (!product::Enabled()) break;
#endif
                std::vector<InstanceHandle> removed;
                { std::lock_guard guard(watcher->handleMutex); removed.swap(watcher->pendingRelease); }
                for (auto handle : removed) cleanup->UnregisterInstance(handle);
#ifndef QS_RESIDENT_NATIVE
                if (iteration!=0 && iteration!=3 && iteration!=11) continue;
#endif
                std::vector<winrt::weak_ref<wx::FrameworkElement>> targets;
                { std::lock_guard guard(watcher->handleMutex);
                    std::erase_if(watcher->candidates,[](auto const& weak) { return !weak.get(); });
                    std::erase_if(watcher->ownButtons,[](auto const& weak) { return !weak.get(); });
                    targets=watcher->candidates;
                }
                // Weak references are resolved only to obtain the dispatcher; the UI inspection runs there.
                for (auto const& weak : targets) {
                    try {
                        if (auto fe=weak.get()) {
                            auto action=fe.Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Low,[weak
#ifdef QS_THEME_FUNCTIONAL
                                ,runtime=watcher->themeRuntime
#endif
                            ] {
                                if (!running) return;
                                try { if (auto element=weak.get()) {
#ifdef QS_THEME_FUNCTIONAL
                                    if (auto list=element.try_as<wc::ListViewBase>(); list && list.CanReorderItems()) runtime->BindDrag(list,[](wc::ListViewBase const& current) {
                                        auto vector=current.ItemsSource().try_as<wf::Collections::IVector<wf::IInspectable>>();
                                        if (vector) Log(L"POSITION SAVE AFTER DRAG error="+std::to_wstring(tileorder::Save(ActionNames(vector))));
                                    });
#endif
#ifdef QS_NATIVE_ACTION_PROTOTYPE
#ifdef QS_CLEANUP_NATIVE
                                    auto list=element.try_as<wc::ListViewBase>();
                                    if (list && element.Name()==L"RootGridView" && list.CanReorderItems() && !prototypeAttempted.exchange(true)) RemoveNativeAction(element);
#else
                                    InstallNativeAction(element);
#endif
#endif
#ifndef QS_RESIDENT_NATIVE
                                    Inspect(element);
#else
                                    Geometry(element);
#endif
                                } } catch (winrt::hresult_error const& error) { Log(L"DELAYED INSPECT ERROR "+std::to_wstring(static_cast<unsigned long>(error.code()))+L" "+std::wstring(error.message())); }
                                catch (...) { Log(L"DELAYED INSPECT ERROR "+std::to_wstring(static_cast<unsigned long>(winrt::to_hresult()))); }
                            });
                            action.get();
                        }
                    } catch (...) { Log(L"DISPATCH ERROR"); }
                }
                std::vector<winrt::weak_ref<wx::FrameworkElement>> buttons;
                { std::lock_guard guard(watcher->handleMutex); buttons=watcher->ownButtons; }
                for (auto const& weak : buttons) {
                    try { if (auto fe=weak.get()) {
                        auto action=fe.Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Low,[weak] {
                            if (!running) return;
                            try { if (auto button=weak.get()) Geometry(button); } catch (...) { Log(L"GEOMETRY ERROR"); }
                        }); action.get();
                    } } catch (...) {}
                }
            }
            if (stopEvent) CloseHandle(stopEvent);
#ifdef QS_THEME_FUNCTIONAL
            if (watcher->themeRuntime) watcher->themeRuntime->Stop();
            std::vector<winrt::weak_ref<wx::FrameworkElement>> cleanupTargets;
            { std::lock_guard guard(watcher->handleMutex); cleanupTargets=watcher->candidates; }
            for (auto const& weak : cleanupTargets) {
                try { if (auto fe=weak.get()) {
                    auto action=fe.Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,[weak] {
                        try { if (auto element=weak.get()) RemoveNativeAction(element); } catch (...) { Log(L"PROTOTYPE CLEANUP ERROR"); }
                    }); action.get();
                } } catch (...) { Log(L"PROTOTYPE CLEANUP DISPATCH ERROR"); }
            }
#endif
            running=false;
            hr=watcher->service->UnadviseVisualTreeChange(watcher.get());
            Log(L"UNADVISE HRESULT="+std::to_wstring(static_cast<unsigned long>(hr)));
            if (SUCCEEDED(hr)) {
                std::vector<InstanceHandle> handles;
                { std::lock_guard guard(watcher->handleMutex); handles.assign(watcher->handles.begin(),watcher->handles.end()); handles.insert(handles.end(),watcher->pendingRelease.begin(),watcher->pendingRelease.end()); watcher->candidates.clear(); watcher->ownButtons.clear(); }
                std::sort(handles.begin(),handles.end()); handles.erase(std::unique(handles.begin(),handles.end()),handles.end());
                for (auto handle : handles) cleanup->UnregisterInstance(handle);
                Log(L"RELEASED DIAGNOSTIC HANDLES="+std::to_wstring(handles.size()));
            }
        }
        // Keep the inert diagnostic DLL mapped until normal Shell exit. This avoids unsafe forced unloading.
        Log(L"SESSION FINISHED");
    } catch (...) {
        auto error=winrt::to_hresult();
#ifdef QS_THEME_FUNCTIONAL
        if (watcher->themeRuntime) watcher->themeRuntime->Stop();
#endif
        running=false; Log(L"SESSION ERROR "+std::to_wstring(static_cast<unsigned long>(error)));
    }
    { std::lock_guard guard(logMutex); if (logFile!=INVALID_HANDLE_VALUE) { CloseHandle(logFile); logFile=INVALID_HANDLE_VALUE; } }
    return 0;
}

struct Tap : winrt::implements<Tap,IObjectWithSite> {
    winrt::com_ptr<IUnknown> site;
    HRESULT STDMETHODCALLTYPE SetSite(IUnknown* value) noexcept override {
        try {
            site.copy_from(value);
            if (!site) return S_OK;
            if (running.exchange(true)) return HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
            prototypeAttempted=false;
            auto diagnostics=site.as<IXamlDiagnostics>();
            BSTR initialization=nullptr;
            winrt::check_hresult(diagnostics->GetInitializationData(&initialization));
            std::wstring path=initialization ? initialization : L""; SysFreeString(initialization);
            { std::lock_guard guard(logMutex); logFile=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr); }
            if (logFile==INVALID_HANDLE_VALUE) { running=false; return HRESULT_FROM_WIN32(GetLastError()); }
            Log(L"PROBE PID="+std::to_wstring(GetCurrentProcessId()));
#ifdef QS_PRODUCT
            DWORD settingsError=0; auto enabled=product::Enabled(&settingsError); auto compatible=product::Compatible();
            Log(L"PRODUCT GATE enabled="+std::to_wstring(enabled)+L" settingsError="+std::to_wstring(settingsError)+L" compatible="+std::to_wstring(compatible));
            if (!enabled || !compatible) {
                running=false; Log(L"SESSION REFUSED: product configuration or image mismatch");
                std::lock_guard guard(logMutex); CloseHandle(logFile); logFile=INVALID_HANDLE_VALUE;
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
#endif
            auto watcher=winrt::make_self<Watcher>(diagnostics);
#ifdef QS_THEME_FUNCTIONAL
            watcher->themeRuntime=std::make_shared<themeui::Runtime>(Log);
            if (!watcher->themeRuntime->Start()) { running=false; return HRESULT_FROM_WIN32(GetLastError()); }
#endif
            watcher->AddRef();
            auto thread=CreateThread(nullptr,0,Session,watcher.get(),0,nullptr);
            if (!thread) {
                auto error=GetLastError();
#ifdef QS_THEME_FUNCTIONAL
                if (watcher->themeRuntime) watcher->themeRuntime->Stop();
#endif
                watcher->Release(); running=false; return HRESULT_FROM_WIN32(error);
            }
            CloseHandle(thread);
            return S_OK;
        } catch (...) { running=false; return winrt::to_hresult(); }
    }
    HRESULT STDMETHODCALLTYPE GetSite(REFIID iid,void** result) noexcept override { return site.as(iid,result); }
};

struct Factory : winrt::implements<Factory,IClassFactory> {
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer,REFIID iid,void** result) noexcept override {
        if (!result) return E_POINTER; *result=nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        try { return winrt::make<Tap>().as(iid,result); } catch (...) { return winrt::to_hresult(); }
    }
    HRESULT STDMETHODCALLTYPE LockServer(BOOL) noexcept override { return S_OK; }
};
}

STDAPI DllGetClassObject(REFCLSID clsid,REFIID iid,void** result) {
    if (!result) return E_POINTER; *result=nullptr;
    if (clsid!=ProbeClsid) return CLASS_E_CLASSNOTAVAILABLE;
    try { return winrt::make<Factory>().as(iid,result); } catch (...) { return winrt::to_hresult(); }
}
STDAPI DllCanUnloadNow() { return S_FALSE; }
