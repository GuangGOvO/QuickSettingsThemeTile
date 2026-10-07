#pragma once
#include <inspectable.h>
// Partial private interfaces, verified against this machine's ControlCenter PDB and live GetIids.
// Only the named, verified members are called. The DLL fingerprint guard is mandatory.
struct __declspec(uuid("CA79615D-BFBA-58DC-8D55-741963985C48")) NativeModelInitialize : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE Initialize() = 0; // slot 6
};
