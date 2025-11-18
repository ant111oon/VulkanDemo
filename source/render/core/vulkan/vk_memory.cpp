#include "pch.h"

#define VMA_IMPLEMENTATION
#include "vk_memory.h"


namespace vkn
{
    Allocator::~Allocator()
    {
        Destroy();
    }


    bool Allocator::Create(const AllocatorCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Device is already initialized");
            return false;
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

        const bool isCreated = m_allocator != VK_NULL_HANDLE;
        VK_ASSERT(isCreated);

        m_pDevice = info.pDevice;
        
        SetCreated(isCreated);
        
        return isCreated;
    }


    void Allocator::Destroy()
    {
        if (!IsCreated()) {
            return;
        }

        vmaDestroyAllocator(m_allocator);
        m_allocator = VK_NULL_HANDLE;

        m_pDevice = nullptr;

        Object::Destroy();
    }
}