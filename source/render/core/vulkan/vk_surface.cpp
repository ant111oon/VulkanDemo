#include "pch.h"

#include "vk_surface.h"


namespace vkn
{
    bool Surface::Create(const SurfaceCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Surface is already initialized");
            return true;
        }

        VK_ASSERT(info.pInstance && *info.pInstance != VK_NULL_HANDLE);
        VK_ASSERT(info.pWndHandle != nullptr);

        m_pInstance = info.pInstance;

    #ifdef ENG_OS_WINDOWS
        VkWin32SurfaceCreateInfoKHR vkWin32SurfCreateInfo = {};
        vkWin32SurfCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        vkWin32SurfCreateInfo.hinstance = GetModuleHandle(nullptr);
        vkWin32SurfCreateInfo.hwnd = (HWND)info.pWndHandle;

        VK_CHECK(vkCreateWin32SurfaceKHR(*m_pInstance, &vkWin32SurfCreateInfo, nullptr, &m_surface));
    #endif

        VK_ASSERT(m_surface != VK_NULL_HANDLE);

        m_flags.set(FLAG_IS_CREATED, true);

        return true;
    }


    void Surface::Destroy()
    {
        if (!IsCreated()) {
            return;
        }
        
        vkDestroySurfaceKHR(*m_pInstance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;

        m_pInstance = nullptr;

        m_flags.set(FLAG_IS_CREATED, false);
    }
}