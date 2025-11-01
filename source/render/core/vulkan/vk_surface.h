#pragma once

#include "vk_object.h"
#include "vk_instance.h"


namespace vkn
{
    struct SurfaceCreateInfo
    {
        Instance* pInstance;
        void* pWndHandle;
    };


    class Surface : public Object
    {
        friend Surface& GetSurface();

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Surface);
        ENG_DECL_CLASS_NO_MOVABLE(Surface);

        ~Surface();

        bool Create(const SurfaceCreateInfo& info);
        void Destroy();

        VkSurfaceKHR Get() const
        {
            VK_ASSERT(IsCreated());
            return m_surface;
        }

    private:
        Surface() = default;

    private:
        Instance* m_pInstance = nullptr;
        VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    };


    ENG_FORCE_INLINE Surface& GetSurface()
    {
        static Surface surface;
        return surface;
    }
}