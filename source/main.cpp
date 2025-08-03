#include "core/wnd_system/wnd_system.h"


int main(int argc, char* argv[])
{
    wndSysInit();
    BaseWindow* pWnd = wndSysGetMainWindow();

    WindowInitInfo wndInitInfo = {};
    wndInitInfo.title = "Vulkan Demo";
    wndInitInfo.width = 980;
    wndInitInfo.height = 640;

    pWnd->Init(wndInitInfo);
    ENG_ASSERT(pWnd->IsInitialized());

    while(!pWnd->IsClosed()) {
        pWnd->ProcessEvents();
        
        WndEvent event;
        while(pWnd->PopEvent(event)) {
        }
    }

    pWnd->Destroy();
    wndSysTerminate();

    return 0;
}