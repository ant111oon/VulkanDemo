#include "pch.h"

#include "vk_semaphore.h"
#include "vk_utils.h"


namespace vkn
{
    Semaphore::~Semaphore()
    {
        Destroy();
    }


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

        std::swap(m_pDevice, semaphore.m_pDevice);
        
        Base::operator=(std::move(semaphore));

        return *this; 
    }


    Semaphore& Semaphore::Create(const SemaphoreCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of semaphore %s", GetDebugName().data());
            Destroy();
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());

        VkSemaphoreCreateInfo semaphoreCreateInfo = {};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCreateInfo.flags = info.flags;

        Base::Create([vkDevice = info.pDevice->Get(), &semaphoreCreateInfo](VkSemaphore& semaphore) {
            VK_CHECK(vkCreateSemaphore(vkDevice, &semaphoreCreateInfo, nullptr, &semaphore));
            return semaphore != VK_NULL_HANDLE;
        });
        
        VK_ASSERT(IsCreated());

        m_pDevice = info.pDevice;

        return *this;
    }


    Semaphore& Semaphore::Create(Device* pDevice, VkSemaphoreCreateFlags flags)
    {
        SemaphoreCreateInfo info = {};
        info.pDevice = pDevice;
        info.flags = flags;

        return Create(info);
    }


    Semaphore& Semaphore::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        Base::Destroy([device = m_pDevice->Get()](VkSemaphore& semaphore) {
            vkDestroySemaphore(device, semaphore, nullptr);
        });

        m_pDevice = nullptr;

        return *this;
    }


    Device& Semaphore::GetDevice() const
    {
        VK_ASSERT(IsCreated());
        return *m_pDevice;
    }
}