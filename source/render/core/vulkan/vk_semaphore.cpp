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
        if (IsCreated()) {
            Destroy();
        }

        std::swap(m_pDevice, semaphore.m_pDevice);
        std::swap(m_semaphore, semaphore.m_semaphore);

    #ifdef ENG_BUILD_DEBUG
        m_debugName.swap(semaphore.m_debugName);
    #endif

        std::swap(m_state, semaphore.m_state);

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
        m_state.set(FLAG_IS_CREATED, isCreated);

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

    #ifdef ENG_BUILD_DEBUG
        m_debugName.fill('\0');
    #endif

        m_state.reset();
    }


    void Semaphore::SetDebugName(const char* pName)
    {
    #ifdef ENG_BUILD_DEBUG
        VK_ASSERT(IsCreated());
        VK_ASSERT(pName);
        
        const size_t nameLength = strlen(pName);
        VK_ASSERT_MSG(nameLength < utils::MAX_VK_OBJ_DBG_NAME_LENGTH, "Debug name %s is too long: %zu (max length: %zu)", pName, nameLength, utils::MAX_VK_OBJ_DBG_NAME_LENGTH - 1);

        m_debugName.fill('\0');
        memcpy_s(m_debugName.data(), m_debugName.size(), pName, nameLength);

        utils::SetObjectName(m_pDevice->Get(), (uint64_t)m_semaphore, VK_OBJECT_TYPE_SEMAPHORE, pName);
    #endif
    }


    const char* Semaphore::GetDebugName() const
    {
    #ifdef ENG_BUILD_DEBUG
        return m_debugName.data();
    #else
        return "Semaphore";
    #endif
    }
}