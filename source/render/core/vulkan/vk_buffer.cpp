#include "pch.h"

#include "vk_buffer.h"
#include "vk_utils.h"


namespace vkn
{
    Buffer::Buffer(Device* pDevice, VkDeviceSize size, VkBufferUsageFlags2 usage, const AllocationInfo& allocInfo)
    {
        Create(pDevice, size, usage, allocInfo);
    }


    Buffer::Buffer(const BufferCreateInfo& info)
    {
        Create(info);
    }


    Buffer::Buffer(Buffer&& buffer) noexcept
    {
        *this = std::move(buffer);
    }


    Buffer::~Buffer()
    {
        Destroy();
    }


    Buffer& Buffer::operator=(Buffer&& buffer) noexcept
    {
        if (this == &buffer) {
            return *this;
        }

        if (IsCreated()) {
            Destroy();
        }

        std::swap(m_pDevice, buffer.m_pDevice);

        std::swap(m_allocation, buffer.m_allocation);
        std::swap(m_allocInfo, buffer.m_allocInfo);
        
        std::swap(m_size, buffer.m_size);

        std::swap(m_deviceAddress, buffer.m_deviceAddress);

        std::swap(m_currStageMask, buffer.m_currStageMask);
        std::swap(m_currAccessMask, buffer.m_currAccessMask);

        std::swap(m_state, buffer.m_state);

        Base::operator=(std::move(buffer));

        return *this; 
    }


    Buffer& Buffer::Create(Device* pDevice, VkDeviceSize size, VkBufferUsageFlags2 usage, const AllocationInfo& allocInfo)
    {
        BufferCreateInfo createInfo = {};
        createInfo.pDevice = pDevice;
        createInfo.size = size;
        createInfo.usage = usage;
        createInfo.pAllocInfo = &allocInfo;

        return Create(createInfo);
    }


    Buffer& Buffer::Create(const BufferCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of buffer %s", GetDebugName().data());
            Destroy();
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());
        VK_ASSERT(GetAllocator().IsCreated());
        VK_ASSERT(info.pAllocInfo);
        VK_ASSERT(info.size > 0);

        VkDevice vkDevice = info.pDevice->Get();

        VkBufferCreateInfo ci = {};
        ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        ci.size = info.size;
        ci.usage = info.usage;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // TODO: fix

        VmaAllocationCreateInfo allocCI = {};
        allocCI.usage = info.pAllocInfo->usage;
        allocCI.flags = info.pAllocInfo->flags;

        Base::Create([&allocation = m_allocation, &ci, &allocCI, &allocInfo = m_allocInfo](VkBuffer& buffer) {
            VK_CHECK(vmaCreateBuffer(GetAllocator().Get(), &ci, &allocCI, &buffer, &allocation, &allocInfo));
            return buffer != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE;
        });

        // vmaCreateBuffer automatically binds buffer and memory if VMA_ALLOCATION_CREATE_DONT_BIND_BIT is not provided

        VK_ASSERT_MSG(Get() != VK_NULL_HANDLE, "Failed to create Vulkan buffer");
        VK_ASSERT_MSG(m_allocation != VK_NULL_HANDLE, "Failed to allocate Vulkan buffer memory");

        if ((info.usage & VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT) != 0) {
            m_state.set(BIT_IS_DEVICE_ADDRESS, true);

            VkBufferDeviceAddressInfo addressInfo = {};
            addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            addressInfo.buffer = Get();
            m_deviceAddress = vkGetBufferDeviceAddress(vkDevice, &addressInfo);
        }

        m_pDevice = info.pDevice;
        m_size = info.size;

        if ((info.usage & VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT) != 0) {
            m_state.set(BIT_IS_DESCRIPTOR_BUFFER, true);
        }

        if ((info.usage & VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT) != 0) {
            m_state.set(BIT_IS_UNIFORM_BUFFER, true);
        } else if ((info.usage & VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT) != 0) {
            m_state.set(BIT_IS_STORAGE_BUFFER, true);
        } else if ((info.usage & VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT) != 0) {
            m_state.set(BIT_IS_INDEX_BUFFER, true);
        } else {
            VK_ASSERT_MSG(m_state.test(BIT_IS_DESCRIPTOR_BUFFER), "Invalid buffer usage type");
        }

        if ((info.pAllocInfo->flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0) {
            m_state.set(BIT_IS_PERSISTENTLY_MAPPED, true);
        }

        return *this;
    }
        

    Buffer& Buffer::CreateConstBuffer(Device* pDevice, VkDeviceSize size, VkBufferUsageFlags2 extraUsageFlags)
    {
        vkn::AllocationInfo allocInfo = {};
        allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        const VkBufferUsageFlags2 usage = 
            VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT | 
            VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_2_TRANSFER_DST_BIT |
            extraUsageFlags;

        return Create(pDevice, size, usage, allocInfo);
    }

    
    Buffer& Buffer::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        VK_ASSERT(!IsMapped());

        m_allocInfo = {};

        m_size = 0;

        m_deviceAddress = 0;

        m_currStageMask = VK_PIPELINE_STAGE_2_NONE;
        m_currAccessMask = VK_ACCESS_2_NONE;

        m_pDevice = nullptr;
        m_state.reset();

        Base::Destroy([&allocation = m_allocation](VkBuffer& buffer) {
            vmaDestroyBuffer(GetAllocator().Get(), buffer, allocation);
            
            allocation = VK_NULL_HANDLE;
            buffer = VK_NULL_HANDLE;
        });

        return *this;
    }


    void* Buffer::Map(VkDeviceSize offset, VkDeviceSize size)
    {
        VK_ASSERT(IsCreated());

        size = size == VK_WHOLE_SIZE ? GetMemorySize() : size;
        VK_ASSERT(offset + size <= GetMemorySize());

        if (IsPersistentlyMapped()) {
            return (void*)((uint8_t*)(m_allocInfo.pMappedData) + offset);
        }
        
        VK_ASSERT(!IsMapped());
    
        void* pData = nullptr;
        VK_CHECK(vmaMapMemory(GetAllocator().Get(), m_allocation, &pData));

        m_state.set(BIT_IS_MAPPED, pData != nullptr);

        return (void*)((uint8_t*)(pData) + offset);
    }


    Buffer& Buffer::Map(void** ppData, VkDeviceSize offset, VkDeviceSize size)
    {
        VK_ASSERT(ppData);
        *ppData = Map(offset, size);

        return *this;
    }


    Buffer& Buffer::Unmap()
    {
        VK_ASSERT(IsCreated());

        if (IsPersistentlyMapped()) {
            VK_LOG_WARN("Attempt to unmap persistently mapped buffer %s", GetDebugName().data());
            return *this;
        }

        VK_ASSERT(IsMapped());

        vmaUnmapMemory(GetAllocator().Get(), m_allocation);

        m_state.set(BIT_IS_MAPPED, false);

        return *this;
    }


    Device& Buffer::GetDevice() const
    {
        VK_ASSERT(IsCreated());
        return *m_pDevice;
    }


    VkDeviceMemory Buffer::GetMemory() const
    {
        VK_ASSERT(IsCreated());
        return m_allocInfo.deviceMemory;
    }


    VkDeviceAddress Buffer::GetDeviceAddress() const
    {
        VK_ASSERT(IsCreated());
        return m_deviceAddress;
    }


    VkDeviceSize Buffer::GetMemorySize() const
    {
        VK_ASSERT(IsCreated());
        return m_size;
    }


    bool Buffer::IsMapped() const
    {
        VK_ASSERT(IsCreated());
        return m_state.test(BIT_IS_MAPPED);
    }


    bool Buffer::IsPersistentlyMapped() const
    {
        VK_ASSERT(IsCreated());
        return m_state.test(BIT_IS_PERSISTENTLY_MAPPED);
    }


    bool Buffer::IsUniformBuffer() const
    {
        VK_ASSERT(IsCreated());
        return m_state.test(BIT_IS_UNIFORM_BUFFER);
    }


    bool Buffer::IsStorageBuffer() const
    {
        VK_ASSERT(IsCreated());
        return m_state.test(BIT_IS_STORAGE_BUFFER);
    }


    bool Buffer::IsIndexBuffer() const
    {
        VK_ASSERT(IsCreated());
        return m_state.test(BIT_IS_INDEX_BUFFER);
    }


    bool Buffer::IsDescriptorBuffer() const
    {
        VK_ASSERT(IsCreated());
        return m_state.test(BIT_IS_DESCRIPTOR_BUFFER);
    }


    bool Buffer::HasDeviceAddress() const
    {
        VK_ASSERT(IsCreated());
        return m_state.test(BIT_IS_DEVICE_ADDRESS);
    }


    void Buffer::Transit(VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccessMask)
    {
        VK_ASSERT(IsCreated());

        m_currStageMask = dstStage;
        m_currAccessMask = dstAccessMask;
    }
}