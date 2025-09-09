#pragma once

#include "vk_object.h"
#include "vk_device.h"


namespace vkn
{
    struct FenceCreateInfo
    {
        Device* pDevice;
        VkFenceCreateFlags flags;
    };


    class Fence : public Object
    {
    public:
        ENG_DECL_CLASS_NO_COPIABLE(Fence);

        Fence() = default;

        Fence(const FenceCreateInfo& info);
        Fence(Device* pDevice, VkFenceCreateFlags flags = VK_FENCE_CREATE_SIGNALED_BIT);

        Fence(Fence&& fence) noexcept;
        Fence& operator=(Fence&& fence) noexcept;

        bool Create(const FenceCreateInfo& info);
        bool Create(Device* pDevice, VkFenceCreateFlags flags = VK_FENCE_CREATE_SIGNALED_BIT);

        void Destroy();

        void Reset();
        void WaitFor(uint64_t timeout);

        void SetDebugName(const char* pName) { Object::SetDebugName(m_pDevice->Get(), (uint64_t)m_fence, VK_OBJECT_TYPE_FENCE, pName); }
        const char* GetDebugName() const { return Object::GetDebugName("Fence"); }

        Device* GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_pDevice;
        }

        VkFence Get() const
        {
            VK_ASSERT(IsCreated());
            return m_fence;
        }

    private:
        Device* m_pDevice = nullptr;

        VkFence m_fence = VK_NULL_HANDLE;
    };
}