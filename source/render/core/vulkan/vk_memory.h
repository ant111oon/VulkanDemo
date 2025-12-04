#pragma once

#include "vk_object.h"
#include "vk_device.h"

#include <vk_mem_alloc.h>


namespace vkn
{
    struct AllocationInfo
    {
        VmaAllocationCreateFlags flags;
        VmaMemoryUsage           usage;
    };


    struct AllocatorCreateInfo
    {
        Device* pDevice;

        VmaAllocatorCreateFlags flags;

        // Preferred size of a single `VkDeviceMemory` block to be allocated from large heaps > 1 GiB. Optional.
        // Set to 0 to use default, which is currently 256 MiB.
        VkDeviceSize preferredLargeHeapBlockSize;
    };


    class Allocator : public Object
    {
        friend Allocator& GetAllocator();

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Allocator);
        ENG_DECL_CLASS_NO_MOVABLE(Allocator);

        ~Allocator();

        Allocator& Create(const AllocatorCreateInfo& info);
        Allocator& Destroy();

        VmaAllocator Get() const
        {
            VK_ASSERT(IsCreated());
            return m_allocator;
        }

        Device* GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_pDevice;
        }

    private:
        Allocator() = default;

    private:
        Device* m_pDevice = nullptr;

        VmaAllocator m_allocator = VK_NULL_HANDLE;
    };


    ENG_FORCE_INLINE Allocator& GetAllocator()
    {
        static Allocator allocator;
        return allocator;
    }
}