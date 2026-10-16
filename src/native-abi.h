#pragma once
#include <inspectable.h>
#include "compatibility.h"
// Partial private interface layout: IInspectable base slots followed by Initialize at slot 6.
// The IID is profile-specific and must come from exact-candidate ABI screening.
struct NativeModelInitialize : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE Initialize() = 0; // slot 6
};

namespace native_abi {
inline compatibility::Profile const* ProfileForImage(WORD machine,DWORD timestamp,DWORD imageSize) noexcept {
    return compatibility::FindByMetadata(machine,timestamp,imageSize);
}
}
