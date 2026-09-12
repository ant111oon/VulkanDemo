#pragma once

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


    class Allocator final
    {
    public:
        static Allocator& Inst();

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Allocator);
        ENG_DECL_CLASS_NO_MOVABLE(Allocator);

        ~Allocator();

        Allocator& Create(const AllocatorCreateInfo& info);
        Allocator& Destroy();

        const VmaAllocator& Get() const;
        Device& GetDevice() const;

        bool IsCreated() const;

    private:
        Allocator() = default;

    private:
        Device* m_pDevice = nullptr;

        VmaAllocator m_allocator = VK_NULL_HANDLE;
    };
}