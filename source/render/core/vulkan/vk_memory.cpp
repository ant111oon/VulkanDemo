#include "pch.h"

#define VMA_IMPLEMENTATION
#include "vk_memory.h"


namespace vkn
{
    Allocator::~Allocator()
    {
        Destroy();
    }


    Allocator& Allocator::Create(const AllocatorCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of Vulan memory allocator");
            Destroy();
        }

        VK_ASSERT(info.pDevice != nullptr);
        VK_ASSERT(info.pDevice->IsCreated());

        VmaAllocatorCreateInfo createInfo = {};
        createInfo.flags = info.flags;
        createInfo.physicalDevice = info.pDevice->GetPhysDevice()->Get();
        createInfo.device = info.pDevice->Get();
        createInfo.preferredLargeHeapBlockSize = info.preferredLargeHeapBlockSize;
        createInfo.instance = info.pDevice->GetPhysDevice()->GetInstance()->Get();
        createInfo.vulkanApiVersion = info.pDevice->GetPhysDevice()->GetInstance()->GetApiVersion();

        m_allocator = VK_NULL_HANDLE;
        VK_CHECK(vmaCreateAllocator(&createInfo, &m_allocator));
        VK_ASSERT(m_allocator != VK_NULL_HANDLE);

        m_pDevice = info.pDevice;
        
        SetCreated(true);
        
        return *this;
    }


    Allocator& Allocator::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        vmaDestroyAllocator(m_allocator);
        m_allocator = VK_NULL_HANDLE;

        m_pDevice = nullptr;

        Object::Destroy();

        return *this;
    }
}