#include "pch.h"

#include "vk_buffer.h"
#include "vk_utils.h"


namespace vkn
{
    static uint32_t FindMemoryType(Device* pDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        const VkPhysicalDeviceMemoryProperties& memProps = pDevice->GetPhysDevice()->GetMemoryProperties();

        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            const VkMemoryPropertyFlags propertyFlags = memProps.memoryTypes[i].propertyFlags;
            
            if ((typeFilter & (1 << i)) && (propertyFlags & properties) == properties) {
                return i;
            }
        }

        return UINT32_MAX;
    }


    Buffer::Buffer(Buffer&& buffer) noexcept
    {
        *this = std::move(buffer);
    }


    Buffer& Buffer::operator=(Buffer&& buffer) noexcept
    {
        if (IsCreated()) {
            Destroy();
        }

        std::swap(m_pDevice, buffer.m_pDevice);

        std::swap(m_buffer, buffer.m_buffer);
        std::swap(m_memory, buffer.m_memory);
        std::swap(m_deviceAddress, buffer.m_deviceAddress);

        std::swap(m_size, buffer.m_size);
        std::swap(m_usage, buffer.m_usage);
        std::swap(m_properties, buffer.m_properties);
        std::swap(m_memAllocFlags, buffer.m_memAllocFlags);

        std::swap(m_state, buffer.m_state);

        return *this; 
    }


    bool Buffer::Create(const BufferCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Buffer is already created");
            return true;
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());

        VkDevice vkDevice = info.pDevice->Get();

        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = info.size;
        bufferCreateInfo.usage = info.usage;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // TODO: fix

        m_buffer = VK_NULL_HANDLE;
        VK_CHECK(vkCreateBuffer(vkDevice, &bufferCreateInfo, nullptr, &m_buffer));

        const bool isBufferCreated = m_buffer != VK_NULL_HANDLE;
        VK_ASSERT(isBufferCreated);

        VkBufferMemoryRequirementsInfo2 memRequirementsInfo = {};
        memRequirementsInfo.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
        memRequirementsInfo.buffer = m_buffer;

        VkMemoryRequirements2 memRequirements = {};
        memRequirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
        vkGetBufferMemoryRequirements2(vkDevice, &memRequirementsInfo, &memRequirements);

        VkMemoryAllocateFlagsInfo memAllocFlagsInfo = {};
        memAllocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        memAllocFlagsInfo.flags = info.memAllocFlags;

        VkMemoryAllocateInfo memAllocInfo = {};
        memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memAllocInfo.pNext = &memAllocFlagsInfo;
        memAllocInfo.allocationSize = memRequirements.memoryRequirements.size;
        memAllocInfo.memoryTypeIndex = FindMemoryType(info.pDevice, memRequirements.memoryRequirements.memoryTypeBits, info.properties);
        VK_ASSERT_MSG(memAllocInfo.memoryTypeIndex != UINT32_MAX, "Failed to find required memory type index");

        m_memory = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateMemory(vkDevice, &memAllocInfo, nullptr, &m_memory));

        const bool isMemoryAllocated = m_memory != VK_NULL_HANDLE;
        VK_ASSERT(isMemoryAllocated);

        VkBindBufferMemoryInfo bindInfo = {};
        bindInfo.sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO;
        bindInfo.buffer = m_buffer;
        bindInfo.memory = m_memory;

        VK_CHECK(vkBindBufferMemory2(vkDevice, 1, &bindInfo));

        if (info.memAllocFlags & VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT) {
            VkBufferDeviceAddressInfo addressInfo = {};
            addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            addressInfo.buffer = m_buffer;
            m_deviceAddress = vkGetBufferDeviceAddress(vkDevice, &addressInfo);
        }

        m_pDevice = info.pDevice;

        m_size = memRequirements.memoryRequirements.size;
        m_usage = info.usage;
        m_properties = info.properties;
        m_memAllocFlags = info.memAllocFlags;

        const bool isCreated = isBufferCreated && isMemoryAllocated;
        VK_ASSERT(isCreated);

    #ifdef ENG_BUILD_DEBUG
        utils::SetObjectName(m_pDevice->Get(), (uint64_t)m_buffer, VK_OBJECT_TYPE_BUFFER, info.pDebugName);
    #else
        (void)info.pDebugName;
    #endif

        m_state.set(FLAG_IS_CREATED, isCreated);

        return isCreated;
    }

    
    void Buffer::Destroy()
    {
        if (!IsCreated()) {
            return;
        }

        VkDevice vkDevice = m_pDevice->Get();

        vkFreeMemory(vkDevice, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
        
        vkDestroyBuffer(vkDevice, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;

        m_pDevice = nullptr;

        m_deviceAddress = 0;
        m_size = 0;

        m_usage = {};
        m_properties = {};
        m_memAllocFlags = {};

        m_state.reset();
    }


    void* Buffer::Map(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags)
    {
        VK_ASSERT(IsCreated());

        void* pData = nullptr;
        VK_CHECK(vkMapMemory(m_pDevice->Get(), m_memory, offset, size, flags, &pData));

        m_state.set(FLAG_IS_MAPPED, pData != nullptr);

        return pData;
    }


    void Buffer::Unmap()
    {
        VK_ASSERT(IsCreated());
        VK_ASSERT(IsMapped());

        vkUnmapMemory(m_pDevice->Get(), m_memory);

        m_state.set(FLAG_IS_MAPPED, false);
    }
}