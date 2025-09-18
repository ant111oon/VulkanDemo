#include "pch.h"

#include "vk_fence.h"
#include "utils/vk_utils.h"


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
        if (this == &fence) {
            return *this;
        }

        if (IsCreated()) {
            Destroy();
        }

        Object::operator=(std::move(fence));

        std::swap(m_pDevice, fence.m_pDevice);
        std::swap(m_fence, fence.m_fence);

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

        SetCreated(isCreated);

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

        Object::Destroy();
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
        Object::SetDebugName(*m_pDevice, (uint64_t)m_fence, VK_OBJECT_TYPE_FENCE, pName);
    }


    const char* Fence::GetDebugName() const
    {
        return Object::GetDebugName("Fence");
    }
}