#include "pch.h"

#include "wnd_system.h"

#include "core/utils/assert.h"


#define WND_ASSERT(COND) ENG_ASSERT_SYSTEM(COND, "WINDOW")


static std::unique_ptr<Window> pWndSysInst = nullptr;


void wndSysInit()
{
    if (pWndSysInst && pWndSysInst->IsInitialized()) {
        return;
    }

    pWndSysInst = AllocateWindow();
    WND_ASSERT(pWndSysInst != nullptr);
}


void wndSysTerminate()
{
    pWndSysInst->Destroy();
    pWndSysInst = nullptr;
}


Window* wndSysGetMainWindow()
{
    WND_ASSERT(pWndSysInst != nullptr);
    return pWndSysInst.get();
}
