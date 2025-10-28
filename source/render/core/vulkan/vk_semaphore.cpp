#include "pch.h"

#include "vk_semaphore.h"
#include "vk_utils.h"


namespace vkn
{
    Semaphore::Semaphore(const SemaphoreCreateInfo& info)
    {
        Create(info);
    }


    Semaphore::Semaphore(Device* pDevice, VkSemaphoreCreateFlags flags)
    {
        Create(pDevice, flags);
    }


    Semaphore::Semaphore(Semaphore&& semaphore) noexcept
    {
        *this = std::move(semaphore);
    }


    Semaphore& Semaphore::operator=(Semaphore&& semaphore) noexcept
    {
        if (this == &semaphore) {
            return *this;
        }

        if (IsCreated()) {
            Destroy();
        }

        Object::operator=(std::move(semaphore));

        std::swap(m_pDevice, semaphore.m_pDevice);
        std::swap(m_semaphore, semaphore.m_semaphore);

        return *this; 
    }


    bool Semaphore::Create(const SemaphoreCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Semaphore %s is already created", GetDebugName());
            return false;
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());

        VkDevice vkDevice = info.pDevice->Get();

        VkSemaphoreCreateInfo semaphoreCreateInfo = {};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCreateInfo.flags = info.flags;

        m_semaphore = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSemaphore(vkDevice, &semaphoreCreateInfo, nullptr, &m_semaphore));

        const bool isCreated = m_semaphore != VK_NULL_HANDLE;
        VK_ASSERT(isCreated);

        m_pDevice = info.pDevice;

        SetCreated(isCreated);

        return isCreated;
    }


    bool Semaphore::Create(Device* pDevice, VkSemaphoreCreateFlags flags)
    {
        SemaphoreCreateInfo info = {};
        info.pDevice = pDevice;
        info.flags = flags;

        return Create(info);
    }


    void Semaphore::Destroy()
    {
        if (!IsCreated()) {
            return;
        }

        vkDestroySemaphore(m_pDevice->Get(), m_semaphore, nullptr);
        m_semaphore = VK_NULL_HANDLE;

        m_pDevice = nullptr;

        Object::Destroy();
    }


    const char* Semaphore::GetDebugName() const
    {
        return Object::GetDebugName("Semaphore");
    }
}