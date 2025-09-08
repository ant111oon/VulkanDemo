#pragma once

#include "vk_core.h"

#include "vk_device.h"


namespace vkn
{
    struct SemaphoreCreateInfo
    {
        Device* pDevice;
        VkSemaphoreCreateFlags flags;
    };


    class Semaphore
    {
    public:
        Semaphore() = default;

        Semaphore(const SemaphoreCreateInfo& info);
        Semaphore(Device* pDevice, VkSemaphoreCreateFlags flags = 0);

        Semaphore(const Semaphore& semaphore) = delete;
        Semaphore& operator=(const Semaphore& semaphore) = delete;

        Semaphore(Semaphore&& semaphore) noexcept;
        Semaphore& operator=(Semaphore&& semaphore) noexcept;

        bool Create(const SemaphoreCreateInfo& info);
        bool Create(Device* pDevice, VkSemaphoreCreateFlags flags = 0);

        void Destroy();

        void SetDebugName(const char* pName);
        const char* GetDebugName() const;

        Device* GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_pDevice;
        }

        VkSemaphore Get() const
        {
            VK_ASSERT(IsCreated());
            return m_semaphore;
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

        VkSemaphore m_semaphore = VK_NULL_HANDLE;

    #ifdef ENG_BUILD_DEBUG
        std::array<char, utils::MAX_VK_OBJ_DBG_NAME_LENGTH> m_debugName = {};
    #endif

        std::bitset<FLAG_COUNT> m_state = {};
    };
}