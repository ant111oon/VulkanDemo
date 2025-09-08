#pragma once

#include "vk_core.h"

#include "vk_instance.h"


namespace vkn
{
    struct SurfaceCreateInfo
    {
        Instance* pInstance;
        void* pWndHandle;
    };


    class Surface
    {
        friend Surface& GetSurface();

    public:
        Surface(const Surface& surf) = delete;
        Surface(Surface&& surf) = delete;

        Surface& operator=(const Surface& surf) = delete;
        Surface& operator=(Surface&& surf) = delete;

        bool Create(const SurfaceCreateInfo& info);
        void Destroy();

        VkSurfaceKHR Get() const
        {
            VK_ASSERT(IsCreated());
            return m_surface;
        }

        bool IsCreated() const { return m_state.test(FLAG_IS_CREATED); }

    private:
        Surface() = default;

    private:
        enum StateFlags
        {
            FLAG_IS_CREATED,
            FLAG_COUNT,
        };

    private:
        Instance* m_pInstance = nullptr;
        VkSurfaceKHR m_surface = VK_NULL_HANDLE;

        std::bitset<FLAG_COUNT> m_state = {};
    };


    ENG_FORCE_INLINE Surface& GetSurface()
    {
        static Surface surface;
        return surface;
    }
}