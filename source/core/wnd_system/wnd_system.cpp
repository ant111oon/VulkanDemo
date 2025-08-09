#include "pch.h"

#include "wnd_system.h"

#include "core/utils/assert.h"


#define WND_ASSERT(COND) ENG_ASSERT_SYSTEM(COND, "WINDOW")


static std::unique_ptr<BaseWindow> pWndSysInst = nullptr;


void wndSysInit()
{
    if (pWndSysInst && pWndSysInst->IsInitialized()) {
        return;
    }

#ifdef ENG_OS_WINDOWS
    pWndSysInst = std::make_unique<Win32Window>();
#endif

    WND_ASSERT(pWndSysInst != nullptr);
}


void wndSysTerminate()
{
    pWndSysInst->Destroy();
    pWndSysInst = nullptr;
}


BaseWindow* wndSysGetMainWindow()
{
    WND_ASSERT(pWndSysInst != nullptr);
    return pWndSysInst.get();
}
