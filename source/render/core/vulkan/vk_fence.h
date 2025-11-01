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
        ~Fence();

        Fence(const FenceCreateInfo& info);
        Fence(Device* pDevice, VkFenceCreateFlags flags = VK_FENCE_CREATE_SIGNALED_BIT);

        Fence(Fence&& fence) noexcept;
        Fence& operator=(Fence&& fence) noexcept;

        bool Create(const FenceCreateInfo& info);
        bool Create(Device* pDevice, VkFenceCreateFlags flags = VK_FENCE_CREATE_SIGNALED_BIT);

        void Destroy();

        void Reset();
        void WaitFor(uint64_t timeout);

        template <typename... Args>
        void SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(*m_pDevice, (uint64_t)m_fence, VK_OBJECT_TYPE_FENCE, pFmt, std::forward<Args>(args)...);
        }

        const char* GetDebugName() const;

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