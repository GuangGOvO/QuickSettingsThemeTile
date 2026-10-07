#include "theme-backend.h"
#include <iostream>

struct FakeStore {
    theme::State state;
    LSTATUS readError=0,systemError=0,appsError=0;
    bool rollbackError=false,externalOnFailure=false,externalOnReadback=false,readbackError=false;
    unsigned systemWrites=0,appsWrites=0,reads=0;
    LSTATUS ReadState(theme::State& value) {
        ++reads;
        if (readError) return readError;
        if (reads>1 && readbackError) return ERROR_ACCESS_DENIED;
        if (reads>1 && externalOnReadback) state.apps=!state.apps;
        value=state; return ERROR_SUCCESS;
    }
    LSTATUS ReadSystem(DWORD& value) { value=state.system; return ERROR_SUCCESS; }
    LSTATUS WriteSystem(DWORD value) {
        ++systemWrites;
        if (systemError) return systemError;
        if (systemWrites>1 && rollbackError) return ERROR_ACCESS_DENIED;
        state.system=value; return ERROR_SUCCESS;
    }
    LSTATUS WriteApps(DWORD value) {
        ++appsWrites;
        if (appsError) {
            if (externalOnFailure) state.system=!state.system;
            return appsError;
        }
        state.apps=value; return ERROR_SUCCESS;
    }
};
int main() {
    unsigned checks=0;
    auto require=[&](bool condition,char const* label) {
        ++checks;
        if (!condition) { std::cerr << "FAIL: " << label << '\n'; ExitProcess(1); }
    };
    for (auto before : {theme::State{0,0},theme::State{1,1},theme::State{0,1},theme::State{1,0}}) {
        FakeStore store; store.state=before;
        auto result=theme::ToggleWithStore(store);
        DWORD target=before.dark()?1:0;
        require(!result.error && result.written && result.actualKnown && result.actual.system==target && result.actual.apps==target,"dark/light/mixed targets");
    }
    {
        FakeStore store; store.readError=ERROR_FILE_NOT_FOUND;
        auto result=theme::ToggleWithStore(store);
        require(result.error==ERROR_FILE_NOT_FOUND && !result.actualKnown && !store.systemWrites && !store.appsWrites,"missing state never writes");
    }
    {
        FakeStore store; store.state={2,0};
        auto result=theme::ToggleWithStore(store);
        require(result.error==ERROR_INVALID_DATA && !result.actualKnown && !store.systemWrites,"invalid state never writes");
    }
    {
        FakeStore store; store.systemError=ERROR_ACCESS_DENIED;
        auto result=theme::ToggleWithStore(store);
        require(result.error==ERROR_ACCESS_DENIED && !result.written && !store.appsWrites && store.state.dark(),"first write failure");
    }
    {
        FakeStore store; store.appsError=ERROR_ACCESS_DENIED;
        auto result=theme::ToggleWithStore(store);
        require(result.error==ERROR_ACCESS_DENIED && !result.written && !result.rollbackFailed && result.actualKnown && store.state.dark() && store.systemWrites==2,"second write failure restores first");
    }
    {
        FakeStore store; store.appsError=ERROR_ACCESS_DENIED; store.rollbackError=true;
        auto result=theme::ToggleWithStore(store);
        require(result.rollbackFailed && result.actualKnown && result.actual.mixed(),"rollback failure exposes actual mixed state");
    }
    {
        FakeStore store; store.appsError=ERROR_ACCESS_DENIED; store.externalOnFailure=true;
        auto result=theme::ToggleWithStore(store);
        require(!result.rollbackFailed && store.systemWrites==1 && store.state.dark(),"external change is preserved");
    }
    {
        FakeStore store; store.externalOnReadback=true;
        auto result=theme::ToggleWithStore(store);
        require(result.error==ERROR_RETRY && result.written && result.actualKnown && result.actual.mixed(),"external change during readback is detected");
    }
    {
        FakeStore store; store.readbackError=true;
        auto result=theme::ToggleWithStore(store);
        require(result.error==ERROR_ACCESS_DENIED && result.written && !result.actualKnown,"readback failure never reports confirmed state");
    }
    std::cout << "PASS: " << checks << " transaction checks; real theme unchanged\n";
}
