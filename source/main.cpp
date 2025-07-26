#include "platform/window.h"

#include "core/utils/assert.h"


int main(int argc, char* argv[])
{
    std::unique_ptr<BaseWindow> pWnd = std::make_unique<Win32Window>();
    
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

    return 0;
}