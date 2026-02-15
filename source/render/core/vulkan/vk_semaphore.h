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
        ~Semaphore();

        Semaphore(const SemaphoreCreateInfo& info);
        Semaphore(Device* pDevice, VkSemaphoreCreateFlags flags = 0);

        Semaphore(Semaphore&& semaphore) noexcept;
        Semaphore& operator=(Semaphore&& semaphore) noexcept;

        Semaphore& Create(const SemaphoreCreateInfo& info);
        Semaphore& Create(Device* pDevice, VkSemaphoreCreateFlags flags = 0);

        Semaphore& Destroy();

        template <typename... Args>
        Semaphore& SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(*GetDevice(), (uint64_t)m_semaphore, VK_OBJECT_TYPE_SEMAPHORE, pFmt, std::forward<Args>(args)...);
            return *this;
        }

        const char* GetDebugName() const;

        Device* GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_pDevice;
        }

        const VkSemaphore& Get() const
        {
            VK_ASSERT(IsCreated());
            return m_semaphore;
        }

    private:
        Device* m_pDevice = nullptr;

        VkSemaphore m_semaphore = VK_NULL_HANDLE;
    };
}