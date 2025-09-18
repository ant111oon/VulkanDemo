#include "pch.h"

#include "vk_mem_allocator.h"

#include "../vk_instance.h"
#include "../vk_device.h"


#define VMA_LOG_TRACE(FMT, ...)        ENG_LOG_TRACE("VMA", FMT, __VA_ARGS__)
#define VMA_LOG_INFO(FMT, ...)         ENG_LOG_INFO("VMA",  FMT, __VA_ARGS__)
#define VMA_LOG_WARN(FMT, ...)         ENG_LOG_WARN("VMA",  FMT, __VA_ARGS__)
#define VMA_LOG_ERROR(FMT, ...)        ENG_LOG_ERROR("VMA", FMT, __VA_ARGS__)

#define VMA_ASSERT_MSG(COND, FMT, ...) ENG_ASSERT_MSG(COND, "VMA", FMT, __VA_ARGS__)
#define VMA_ASSERT(COND)               VMA_ASSERT_MSG(COND, #COND)
#define VMA_ASSERT_FAIL(FMT, ...)      VMA_ASSERT_MSG(false, FMT, __VA_ARGS__)


namespace vkn
{
    bool MemAllocator::Create(const MemAllocatorCreateInfo& info)
    {
        if (IsCreated()) {
            VMA_LOG_WARN("Vulkan memory allocator is already created");
            return false;
        }

        VMA_ASSERT(info.pDevice && info.pDevice->IsCreated());

        VmaAllocatorCreateInfo createInfo = {};
        createInfo.instance = info.pDevice->GetPhysDevice()->GetInstance()->Get();
        createInfo.device = info.pDevice->Get();
        createInfo.physicalDevice = info.pDevice->GetPhysDevice()->Get();
        createInfo.flags = info.flags;

        m_allocator = VK_NULL_HANDLE;
        VK_CHECK(vmaCreateAllocator(&createInfo, &m_allocator));

        const bool isCreated = m_allocator != VK_NULL_HANDLE;
        VMA_ASSERT(isCreated);

        m_pDevice = info.pDevice;

        SetCreated(isCreated);

        return isCreated;
    }


    void MemAllocator::Destroy()
    {
        if (!IsCreated()) {
            return;
        }

        vmaDestroyAllocator(m_allocator);
        m_allocator = VK_NULL_HANDLE;

        Object::Destroy();
    }
}