#pragma once

#include "vk_device.h"


namespace vkn
{
    struct FenceCreateInfo
    {
        Device* pDevice;
        VkFenceCreateFlags flags;
    };


    class Fence : public Handle<VkFence>
    {
    public:
        using Base = Handle<VkFence>;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Fence);

        Fence() = default;
        ~Fence();

        Fence(const FenceCreateInfo& info);
        Fence(Device* pDevice, VkFenceCreateFlags flags = VK_FENCE_CREATE_SIGNALED_BIT);

        Fence(Fence&& fence) noexcept;
        Fence& operator=(Fence&& fence) noexcept;

        Fence& Create(const FenceCreateInfo& info);
        Fence& Create(Device* pDevice, VkFenceCreateFlags flags = VK_FENCE_CREATE_SIGNALED_BIT);

        Fence& Destroy();

        Fence& Reset();
        Fence& WaitFor(uint64_t timeout);

        VkResult GetStatus() const;
        const Fence& GetStatus(VkResult& status) const;

        Device& GetDevice() const;

    private:
        Device* m_pDevice = nullptr;
    };
}