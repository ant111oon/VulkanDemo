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

        std::swap(m_pDevice, fence.m_pDevice);
        
        Base::operator=(std::move(fence));

        return *this; 
    }


    Fence& Fence::Create(const FenceCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of fence %s", GetDebugName().data());
            Destroy();
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());

        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = info.flags;

        Base::Create([vkDevice = info.pDevice->Get(), &fenceCreateInfo](VkFence& fence) {
            VK_CHECK(vkCreateFence(vkDevice, &fenceCreateInfo, nullptr, &fence));
            return fence != VK_NULL_HANDLE;
        });

        VK_ASSERT(IsCreated());

        m_pDevice = info.pDevice;

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

        Base::Destroy([vkDevice = m_pDevice->Get()](VkFence& fence) {
            vkDestroyFence(vkDevice, fence, nullptr);
        });

        m_pDevice = nullptr;

        return *this;
    }


    Fence& Fence::Reset()
    {
        VK_ASSERT(IsCreated());
        VK_CHECK(vkResetFences(m_pDevice->Get(), 1, &Get()));
        
        return *this;
    }


    Fence& Fence::WaitFor(uint64_t timeout)
    {
        VK_ASSERT(IsCreated());
        VK_CHECK(vkWaitForFences(m_pDevice->Get(), 1, &Get(), VK_TRUE, timeout));
    
        return *this;
    }


    VkResult Fence::GetStatus() const
    {
        VK_ASSERT(IsCreated());
        return vkGetFenceStatus(m_pDevice->Get(), Get());
    }


    const Fence& Fence::GetStatus(VkResult& status) const
    {
        status = GetStatus();
        return *this;
    }
    

    Device& Fence::GetDevice() const
    {
        VK_ASSERT(IsCreated());
        return *m_pDevice;
    }
}