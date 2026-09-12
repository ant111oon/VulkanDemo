#pragma once

#include "vk_device.h"


namespace vkn
{
    struct SemaphoreCreateInfo
    {
        Device* pDevice;
        VkSemaphoreCreateFlags flags;
    };


    class Semaphore final : public DeviceResource<VkSemaphore>
    {
    public:
        ENG_DECL_CLASS_NO_COPIABLE(Semaphore);

        Semaphore() = default;
        ~Semaphore();

        Semaphore(const SemaphoreCreateInfo& info);
        Semaphore(Device* pDevice, VkSemaphoreCreateFlags flags = 0);

        Semaphore(Semaphore&& semaphore) noexcept;
        Semaphore& operator=(Semaphore&& semaphore) noexcept;

        Semaphore& Create(const SemaphoreCreateInfo& info);
        Semaphore& Create(Device* pDevice, VkSemaphoreCreateFlags flags = 0);

        Semaphore& Destroy();

    private:
        using Base = DeviceResource<VkSemaphore>;
    };
}