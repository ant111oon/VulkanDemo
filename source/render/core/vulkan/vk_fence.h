#pragma once

#include "vk_core.h"

#include "vk_device.h"

#include <span>


namespace vkn
{
    struct FenceCreateInfo
    {
        Device* pDevice;
        VkFenceCreateFlags flags;
    };


    class Fence
    {
    public:
        Fence() = default;

        Fence(const FenceCreateInfo& info);
        Fence(Device* pDevice, VkFenceCreateFlags flags = VK_FENCE_CREATE_SIGNALED_BIT);

        Fence(const Fence& fence) = delete;
        Fence& operator=(const Fence& fence) = delete;

        Fence(Fence&& fence) noexcept;
        Fence& operator=(Fence&& fence) noexcept;

        bool Create(const FenceCreateInfo& info);
        bool Create(Device* pDevice, VkFenceCreateFlags flags = VK_FENCE_CREATE_SIGNALED_BIT);

        void Destroy();

        void Reset();
        void WaitFor(uint64_t timeout);

        void SetDebugName(const char* pName);
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

        bool IsCreated() const { return m_state.test(FLAG_IS_CREATED); }

    private:
        enum StateFlags
        {
            FLAG_IS_CREATED,
            FLAG_COUNT,
        };

    private:
        Device* m_pDevice = nullptr;

        VkFence m_fence = VK_NULL_HANDLE;

    #ifdef ENG_BUILD_DEBUG
        std::array<char, utils::MAX_VK_OBJ_DBG_NAME_LENGTH> m_debugName = {};
    #endif

        std::bitset<FLAG_COUNT> m_state = {};
    };
}