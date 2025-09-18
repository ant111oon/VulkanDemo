#pragma once

#include "vk_object.h"
#include "vk_device.h"


namespace vkn
{
    struct SemaphoreCreateInfo
    {
        Device* pDevice;
        VkSemaphoreCreateFlags flags;
    };


    class Semaphore : public Object
    {
    public:
        ENG_DECL_CLASS_NO_COPIABLE(Semaphore);

        Semaphore() = default;

        Semaphore(const SemaphoreCreateInfo& info);
        Semaphore(Device* pDevice, VkSemaphoreCreateFlags flags = 0);

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

    private:
        Device* m_pDevice = nullptr;

        VkSemaphore m_semaphore = VK_NULL_HANDLE;
    };
}