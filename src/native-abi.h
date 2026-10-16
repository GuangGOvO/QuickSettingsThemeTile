#pragma once
#include <inspectable.h>
#include "compatibility.h"
// Partial private interface verified against the ControlCenter PDB and GetIids.
// Build 26300.9711 was rechecked by isolated WinRT activation and QueryInterface;
// only the exact DLL fingerprints below are accepted by the product loader.
struct __declspec(uuid("CA79615D-BFBA-58DC-8D55-741963985C48")) NativeModelInitialize : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE Initialize() = 0; // slot 6
};

namespace native_abi {
inline compatibility::Profile const* ProfileForImage(WORD machine,DWORD timestamp,DWORD imageSize) noexcept {
    return compatibility::FindByMetadata(machine,timestamp,imageSize);
}
}
