#pragma once

#include "vk_instance.h"


namespace vkn
{
    struct SurfaceCreateInfo
    {
        Instance* pInstance;
        void* pWndHandle;
    };


    class Surface final : public Handle<VkSurfaceKHR>
    {
    public:
        static Surface& Inst();

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Surface);
        ENG_DECL_CLASS_NO_MOVABLE(Surface);

        ~Surface();

        Surface& Create(const SurfaceCreateInfo& info);
        Surface& Destroy();

    private:
        Surface() = default;

    private:
        using Base = Handle<VkSurfaceKHR>;

        Instance* m_pInstance = nullptr;
    };
}