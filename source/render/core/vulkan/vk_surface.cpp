#include "pch.h"

#include "vk_surface.h"
#include "vk_instance.h"


namespace vkn
{
    bool Surface::Create(const SurfaceCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Surface is already initialized");
            return true;
        }

        VK_ASSERT(info.pInstance && info.pInstance->IsCreated());
        VK_ASSERT(info.pWndHandle != nullptr);

        m_pInstance = info.pInstance;

    #ifdef ENG_OS_WINDOWS
        VkWin32SurfaceCreateInfoKHR vkWin32SurfCreateInfo = {};
        vkWin32SurfCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        vkWin32SurfCreateInfo.hinstance = GetModuleHandle(nullptr);
        vkWin32SurfCreateInfo.hwnd = (HWND)info.pWndHandle;

        VK_CHECK(vkCreateWin32SurfaceKHR(m_pInstance->Get(), &vkWin32SurfCreateInfo, nullptr, &m_surface));
    #endif

        const bool isCreated = m_surface != VK_NULL_HANDLE;
        VK_ASSERT(isCreated);

        m_state.set(FLAG_IS_CREATED, isCreated);

        return isCreated;
    }


    void Surface::Destroy()
    {
        if (!IsCreated()) {
            return;
        }
        
        vkDestroySurfaceKHR(m_pInstance->Get(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;

        m_pInstance = nullptr;

        m_state.reset();
    }
}