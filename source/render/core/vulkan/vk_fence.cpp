#include "pch.h"

#include "vk_fence.h"
#include "vk_utils.h"


namespace vkn
{
    Fence::Fence(const FenceCreateInfo& info)
    {
        Create(info);
    }


    Fence::Fence(Device* pDevice, VkFenceCreateFlags flags)
    {
        Create(pDevice, flags);
    }


    Fence::Fence(Fence&& fence) noexcept
    {
        *this = std::move(fence);
    }


    Fence& Fence::operator=(Fence&& fence) noexcept
    {
        if (IsCreated()) {
            Destroy();
        }

        std::swap(m_pDevice, fence.m_pDevice);
        std::swap(m_fence, fence.m_fence);

    #ifdef ENG_BUILD_DEBUG
        m_debugName.swap(fence.m_debugName);
    #endif

        std::swap(m_state, fence.m_state);

        return *this; 
    }


    bool Fence::Create(const FenceCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Fence %s is already created", GetDebugName());
            return false;
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());

        VkDevice vkDevice = info.pDevice->Get();

        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = info.flags;

        m_fence = VK_NULL_HANDLE;
        VK_CHECK(vkCreateFence(vkDevice, &fenceCreateInfo, nullptr, &m_fence));

        const bool isCreated = m_fence != VK_NULL_HANDLE;
        VK_ASSERT(isCreated);

        m_pDevice = info.pDevice;
        m_state.set(FLAG_IS_CREATED, isCreated);

        return isCreated;
    }


    bool Fence::Create(Device* pDevice, VkFenceCreateFlags flags)
    {
        FenceCreateInfo info = {};
        info.pDevice = pDevice;
        info.flags = flags;

        return Create(info);
    }


    void Fence::Destroy()
    {
        if (!IsCreated()) {
            return;
        }

        vkDestroyFence(m_pDevice->Get(), m_fence, nullptr);
        m_fence = VK_NULL_HANDLE;

        m_pDevice = nullptr;

    #ifdef ENG_BUILD_DEBUG
        m_debugName.fill('\0');
    #endif

        m_state.reset();
    }


    void Fence::Reset()
    {
        VK_ASSERT(IsCreated());
        VK_CHECK(vkResetFences(m_pDevice->Get(), 1, &m_fence));
    }


    void Fence::WaitFor(uint64_t timeout)
    {
        VK_ASSERT(IsCreated());
        VK_CHECK(vkWaitForFences(m_pDevice->Get(), 1, &m_fence, VK_TRUE, timeout));
    }


    void Fence::SetDebugName(const char* pName)
    {
    #ifdef ENG_BUILD_DEBUG
        VK_ASSERT(IsCreated());
        VK_ASSERT(pName);
        
        const size_t nameLength = strlen(pName);
        VK_ASSERT_MSG(nameLength < utils::MAX_VK_OBJ_DBG_NAME_LENGTH, "Debug name %s is too long: %zu (max length: %zu)", pName, nameLength, utils::MAX_VK_OBJ_DBG_NAME_LENGTH - 1);

        m_debugName.fill('\0');
        memcpy_s(m_debugName.data(), m_debugName.size(), pName, nameLength);

        utils::SetObjectName(m_pDevice->Get(), (uint64_t)m_fence, VK_OBJECT_TYPE_FENCE, pName);
    #endif
    }


    const char* Fence::GetDebugName() const
    {
    #ifdef ENG_BUILD_DEBUG
        return m_debugName.data();
    #else
        return "Fence";
    #endif
    }
}