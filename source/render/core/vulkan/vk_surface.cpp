#include "pch.h"

#include "vk_surface.h"


namespace vkn
{
    Surface::~Surface()
    {
        Destroy();
    }


    Surface& Surface::Create(const SurfaceCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of Vulkan surface");
            Destroy();
        }

        VK_ASSERT(info.pInstance && info.pInstance->IsCreated());
        VK_ASSERT(info.pWndHandle != nullptr);

    #ifdef ENG_OS_WINDOWS
        VkWin32SurfaceCreateInfoKHR vkWin32SurfCreateInfo = {};
        vkWin32SurfCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        vkWin32SurfCreateInfo.hinstance = GetModuleHandle(nullptr);
        vkWin32SurfCreateInfo.hwnd = (HWND)info.pWndHandle;

        Base::Create([vkInst = info.pInstance->Get(), &vkWin32SurfCreateInfo](VkSurfaceKHR& surface) {
            VK_CHECK(vkCreateWin32SurfaceKHR(vkInst, &vkWin32SurfCreateInfo, nullptr, &surface));
            return surface != VK_NULL_HANDLE;
        });
    #endif

        VK_ASSERT(IsCreated());

        m_pInstance = info.pInstance;

        return *this;
    }


    Surface& Surface::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }
        
        Base::Destroy([vkInst = m_pInstance->Get()](VkSurfaceKHR& surface) {
            vkDestroySurfaceKHR(vkInst, surface, nullptr);
        });

        m_pInstance = nullptr;

        return *this;
    }
}