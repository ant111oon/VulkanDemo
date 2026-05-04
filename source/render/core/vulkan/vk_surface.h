#pragma once

#include "vk_instance.h"


namespace vkn
{
    struct SurfaceCreateInfo
    {
        Instance* pInstance;
        void* pWndHandle;
    };


    class Surface : public Handle<VkSurfaceKHR>
    {
        friend Surface& GetSurface();

    public:
        using Base = Handle<VkSurfaceKHR>;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Surface);
        ENG_DECL_CLASS_NO_MOVABLE(Surface);

        ~Surface();

        Surface& Create(const SurfaceCreateInfo& info);
        Surface& Destroy();

    private:
        Surface() = default;

    private:
        Instance* m_pInstance = nullptr;
    };


    ENG_FORCE_INLINE Surface& GetSurface()
    {
        static Surface surface;
        return surface;
    }
}