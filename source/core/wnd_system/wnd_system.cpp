#include "pch.h"

#include "wnd_system.h"

#include "core/utils/assert.h"


#define ENG_WND_ASSERT(COND) ENG_ASSERT_PREFIX(COND, "WINDOW")


static std::unique_ptr<BaseWindow> pWndSysInst = nullptr;


void wndSysInit()
{
    if (pWndSysInst && pWndSysInst->IsInitialized()) {
        return;
    }

#ifdef ENG_OS_WINDOWS
    pWndSysInst = std::make_unique<Win32Window>();
#endif

    ENG_WND_ASSERT(pWndSysInst != nullptr);
}


void wndSysTerminate()
{
    pWndSysInst->Destroy();
    pWndSysInst = nullptr;
}


BaseWindow* wndSysGetMainWindow()
{
    ENG_WND_ASSERT(pWndSysInst != nullptr);
    return pWndSysInst.get();
}
