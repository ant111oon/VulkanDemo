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
        
        Base::operator=(std::move(fence));

        return *this; 
    }


    Fence& Fence::Create(const FenceCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of fence %s", GetDebugName().data());
            Destroy();
        }

        Device* pDevice = info.pDevice;

        VK_ASSERT(pDevice && pDevice->IsCreated());

        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = info.flags;

        Base::Create(pDevice, [vkDevice = pDevice->Get(), &fenceCreateInfo](VkFence& fence) {
            VK_CHECK(vkCreateFence(vkDevice, &fenceCreateInfo, nullptr, &fence));
            return fence != VK_NULL_HANDLE;
        });

        VK_ASSERT(IsCreated());

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

        Base::Destroy([device = GetDevice().Get()](VkFence& fence) {
            vkDestroyFence(device, fence, nullptr);
        });

        return *this;
    }


    Fence& Fence::Reset()
    {
        VK_ASSERT(IsCreated());
        VK_CHECK(vkResetFences(GetDevice().Get(), 1, &Get()));
        
        return *this;
    }


    Fence& Fence::WaitFor(uint64_t timeout)
    {
        VK_ASSERT(IsCreated());
        VK_CHECK(vkWaitForFences(GetDevice().Get(), 1, &Get(), VK_TRUE, timeout));
    
        return *this;
    }


    VkResult Fence::GetStatus() const
    {
        VK_ASSERT(IsCreated());
        return vkGetFenceStatus(GetDevice().Get(), Get());
    }


    const Fence& Fence::GetStatus(VkResult& status) const
    {
        status = GetStatus();
        return *this;
    }
}