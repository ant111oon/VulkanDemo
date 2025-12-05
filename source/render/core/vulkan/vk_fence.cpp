#include "pch.h"

#include "vk_fence.h"
#include "vk_utils.h"


namespace vkn
{
    Fence::~Fence()
    {
        Destroy();
    }

    
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


    Fence& Fence::Create(const FenceCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of fence %s", GetDebugName());
            Destroy();
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());

        VkDevice vkDevice = info.pDevice->Get();

        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = info.flags;

        m_fence = VK_NULL_HANDLE;
        VK_CHECK(vkCreateFence(vkDevice, &fenceCreateInfo, nullptr, &m_fence));
        VK_ASSERT(m_fence != VK_NULL_HANDLE);

        m_pDevice = info.pDevice;

        SetCreated(true);

        return *this;
    }


    Fence& Fence::Create(Device* pDevice, VkFenceCreateFlags flags)
    {
        FenceCreateInfo info = {};
        info.pDevice = pDevice;
        info.flags = flags;

        return Create(info);
    }


    Fence& Fence::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        vkDestroyFence(m_pDevice->Get(), m_fence, nullptr);
        m_fence = VK_NULL_HANDLE;

        m_pDevice = nullptr;

        Object::Destroy();

        return *this;
    }


    Fence& Fence::Reset()
    {
        VK_ASSERT(IsCreated());
        VK_CHECK(vkResetFences(m_pDevice->Get(), 1, &m_fence));
        
        return *this;
    }


    Fence& Fence::WaitFor(uint64_t timeout)
    {
        VK_ASSERT(IsCreated());
        VK_CHECK(vkWaitForFences(m_pDevice->Get(), 1, &m_fence, VK_TRUE, timeout));
    
        return *this;
    }


    VkResult Fence::GetStatus() const
    {
        VK_ASSERT(IsCreated());
        return vkGetFenceStatus(m_pDevice->Get(), m_fence);
    }


    const Fence& Fence::GetStatus(VkResult& status) const
    {
        status = GetStatus();
        return *this;
    }


    const char* Fence::GetDebugName() const
    {
        return Object::GetDebugName("Fence");
    }
}