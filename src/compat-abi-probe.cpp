#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <inspectable.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <iostream>
#include <filesystem>
#include "native-abi.h"

namespace wf = winrt::Windows::Foundation;

int wmain(int argc,wchar_t** argv) {
    if (argc!=3 && argc!=4) {
        std::wcerr << L"Usage: compat-abi-probe.exe <ControlCenter.dll> <expected-Initialize-RVA> [expected-IID]\n";
        return 64;
    }
    wchar_t* end=nullptr;
    auto expectedRva=wcstoul(argv[2],&end,0);
    if (!end || *end || expectedRva>MAXDWORD) {
        std::wcerr << L"expected-Initialize-RVA must be a valid 32-bit integer.\n";
        return 64;
    }
    IID expectedIid{};
    bool hasExpectedIid=false;
    if (argc==4) {
        if (FAILED(CLSIDFromString(argv[3],&expectedIid))) {
            std::wcerr << L"expected-IID must be a valid GUID.\n";
            return 64;
        }
        hasExpectedIid=true;
    }

    auto path=std::filesystem::path(argv[1]);
    auto image=compatibility::InspectPath(path);
    if (image.status==compatibility::Status::InvalidImage || image.status==compatibility::Status::ReadError) {
        std::wcout << L"{\"probeStatus\":\"input_error\",\"error\":" << image.error << L"}\n";
        return 2;
    }

    auto module=LoadLibraryExW(path.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module) {
        std::wcout << L"{\"probeStatus\":\"load_failed\",\"error\":" << GetLastError()
            << L",\"sha256\":\"" << compatibility::Hex(image.facts.sha256) << L"\"}\n";
        return 2;
    }

    bool apartmentInitialized=false,activated=false,iidReported=false,querySucceeded=false,moduleTarget=false;
    uint32_t actualRva=0;
    HRESULT activationHr=E_FAIL,queryHr=E_NOINTERFACE;
    std::vector<IID> implementedIids;
    std::vector<IID> matchedIids;
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        apartmentInitialized=true;
        using FactoryFn=HRESULT (WINAPI*)(HSTRING,void**);
        auto getFactory=reinterpret_cast<FactoryFn>(GetProcAddress(module,"DllGetActivationFactory"));
        if (getFactory) {
            auto className=winrt::hstring{L"ControlCenter.QuickActionViewModel"};
            wf::IActivationFactory factory{nullptr};
            activationHr=getFactory(static_cast<HSTRING>(winrt::get_abi(className)),winrt::put_abi(factory));
            if (SUCCEEDED(activationHr) && factory) {
                auto instance=factory.ActivateInstance<wf::IInspectable>();
                activated=static_cast<bool>(instance);
                if (activated) {
                    auto abi=reinterpret_cast<::IInspectable*>(winrt::get_abi(instance));
                    ULONG iidCount=0; IID* iids=nullptr;
                    if (SUCCEEDED(abi->GetIids(&iidCount,&iids))) {
                        for (ULONG i=0;i<iidCount;++i) implementedIids.push_back(iids[i]);
                        CoTaskMemFree(iids);
                    }

                    std::vector<IID> candidates;
                    if (hasExpectedIid) candidates.push_back(expectedIid);
                    else candidates=implementedIids;
                    for (auto const& candidateIid:candidates) {
                        NativeModelInitialize* initialize=nullptr;
                        auto currentHr=abi->QueryInterface(candidateIid,reinterpret_cast<void**>(&initialize));
                        if (FAILED(currentHr) || !initialize) {
                            queryHr=currentHr;
                            continue;
                        }
                        queryHr=currentHr;
                        querySucceeded=true;
                        auto const& table=*reinterpret_cast<void***>(initialize);
                        auto target=reinterpret_cast<uintptr_t>(table[6]);
                        auto base=reinterpret_cast<uintptr_t>(module);
                        auto isModuleTarget=target>=base && target<base+image.facts.imageSize;
                        auto candidateRva=isModuleTarget?static_cast<uint32_t>(target-base):0;
                        if (isModuleTarget && candidateRva==expectedRva) {
                            matchedIids.push_back(candidateIid);
                            actualRva=candidateRva;
                            moduleTarget=true;
                            iidReported=std::any_of(implementedIids.begin(),implementedIids.end(),[&](IID const& iid){return IsEqualIID(iid,candidateIid);});
                        }
                        initialize->Release();
                    }
                }
            }
        }
    } catch (winrt::hresult_error const& error) {
        activationHr=error.code();
    } catch (...) {
        activationHr=E_UNEXPECTED;
    }
    if (apartmentInitialized) winrt::uninit_apartment();

    auto host=compatibility::ReadHostVersion();
    bool rvaMatches=moduleTarget && actualRva==expectedRva;
    bool passed=activated && iidReported && querySucceeded && rvaMatches && matchedIids.size()==1;
    auto iidText=[](IID const& iid) {
        wchar_t value[40]{};
        StringFromGUID2(iid,value,ARRAYSIZE(value));
        return std::wstring(value);
    };
    std::wcout << L"{\"probeStatus\":\"" << (passed?L"abi_contract_match":L"abi_contract_mismatch")
        << L"\",\"profileStatus\":\"" << compatibility::StatusName(image.status)
        << L"\",\"supportedByProfileTable\":" << (image.supported()?L"true":L"false")
        << L",\"sha256\":\"" << compatibility::Hex(image.facts.sha256)
        << L"\",\"peMachine\":\"" << compatibility::HexValue(image.facts.machine)
        << L"\",\"peTimestamp\":\"" << compatibility::HexValue(image.facts.timestamp)
        << L"\",\"imageSize\":\"" << compatibility::HexValue(image.facts.imageSize)
        << L"\",\"pdbGuid\":\"" << (image.facts.hasPdbIdentity?compatibility::PdbGuidText(image.facts.pdbGuid):L"")
        << L"\",\"pdbAge\":" << image.facts.pdbAge
        << L",\"probeHostBuild\":" << host.build
        << L",\"classActivated\":" << (activated?L"true":L"false")
        << L",\"initializeIidReported\":" << (iidReported?L"true":L"false")
        << L",\"queryInterfaceSucceeded\":" << (querySucceeded?L"true":L"false")
        << L",\"initializeSlot\":6,\"actualInitializeRva\":\"" << compatibility::HexValue(actualRva)
        << L"\",\"expectedInitializeRva\":\"" << compatibility::HexValue(expectedRva)
        << L"\",\"initializeCalled\":false,\"shellTouched\":false"
        << L",\"matchedInitializeIid\":\"" << (matchedIids.size()==1?iidText(matchedIids.front()):L"") << L"\""
        << L",\"matchedInitializeIidCount\":" << matchedIids.size()
        << L",\"interfaceIids\":[";
    for (size_t i=0;i<implementedIids.size();++i) {
        if (i) std::wcout << L",";
        std::wcout << L"\"" << iidText(implementedIids[i]) << L"\"";
    }
    std::wcout << L"]"
        << L",\"activationHresult\":\"" << compatibility::HexValue(static_cast<uint32_t>(activationHr))
        << L"\",\"queryHresult\":\"" << compatibility::HexValue(static_cast<uint32_t>(queryHr)) << L"\"}\n";
    return passed?0:2;
}
