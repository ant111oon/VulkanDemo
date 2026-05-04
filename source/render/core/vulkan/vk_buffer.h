#pragma once

#include "vk_device.h"
#include "vk_memory.h"

#include "vk_resource_access_tracker.h"


namespace vkn
{
    struct BufferCreateInfo
    {
        Device* pDevice;

        VkDeviceSize size;
        VkBufferUsageFlags2 usage;

        const AllocationInfo* pAllocInfo;
    };


    class Buffer : public Handle<VkBuffer>
    {
        friend class CmdBuffer;

    public:
        using Base = Handle<VkBuffer>;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Buffer);

        Buffer() = default;
        Buffer(const BufferCreateInfo& info);
        Buffer(Device* pDevice, VkDeviceSize size, VkBufferUsageFlags2 usage, const AllocationInfo& allocInfo);

        ~Buffer();

        Buffer(Buffer&& buffer) noexcept;
        Buffer& operator=(Buffer&& buffer) noexcept;

        Buffer& Create(const BufferCreateInfo& info);
        Buffer& Create(Device* pDevice, VkDeviceSize size, VkBufferUsageFlags2 usage, const AllocationInfo& allocInfo);
        
        Buffer& CreateConstBuffer(Device* pDevice, VkDeviceSize size, VkBufferUsageFlags2 extraUsageFlags = 0);

        Buffer& Destroy();

        void* Map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
        Buffer& Map(void** ppData, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

        Buffer& Unmap();

        Device& GetDevice() const;

        VkDeviceMemory GetMemory() const;
        VkDeviceAddress GetDeviceAddress() const;
        VkDeviceSize GetMemorySize() const;

        bool IsMapped() const;
        bool IsPersistentlyMapped() const;
        bool IsUniformBuffer() const;
        bool IsStorageBuffer() const;
        bool IsIndexBuffer() const;
        bool IsDescriptorBuffer() const;
        bool HasDeviceAddress() const;

    private:
        BufferAccessTracker& GetAccessTracker();
        const BufferAccessTracker& GetAccessTracker() const;

    private:
        enum StateBits
        {
            BIT_IS_STORAGE_BUFFER,
            BIT_IS_UNIFORM_BUFFER,
            BIT_IS_INDEX_BUFFER,
            BIT_IS_DESCRIPTOR_BUFFER,
            BIT_IS_MAPPED,
            BIT_IS_PERSISTENTLY_MAPPED,
            BIT_IS_DEVICE_ADDRESS,
            BIT_COUNT,
        };

    private:
        Device* m_pDevice = nullptr;

        VmaAllocation m_allocation = VK_NULL_HANDLE;
        VmaAllocationInfo m_allocInfo = {};

        VkDeviceSize m_size = 0;
        VkDeviceAddress m_deviceAddress = 0;

        BufferAccessTracker m_accessTracker = {};

        std::bitset<BIT_COUNT> m_state = {};
    };
}