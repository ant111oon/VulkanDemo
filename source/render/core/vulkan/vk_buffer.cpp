#include "pch.h"

#include "vk_buffer.h"
#include "vk_utils.h"


namespace vkn
{
    Buffer::Buffer(const BufferCreateInfo& info)
        : Object()
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

        std::swap(m_buffer, buffer.m_buffer);
        std::swap(m_allocation, buffer.m_allocation);
        std::swap(m_allocInfo, buffer.m_allocInfo);
        
        std::swap(m_deviceAddress, buffer.m_deviceAddress);

        std::swap(m_state, buffer.m_state);

        Object::operator=(std::move(buffer));

        return *this; 
    }


    bool Buffer::Create(const BufferCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Buffer %s is already created", GetDebugName());
            return false;
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());
        VK_ASSERT(GetAllocator().IsCreated());
        VK_ASSERT(info.pAllocInfo);

        VkDevice vkDevice = info.pDevice->Get();

        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = info.size;
        bufferCreateInfo.usage = info.usage;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // TODO: fix

        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = info.pAllocInfo->usage;
        allocCreateInfo.flags = info.pAllocInfo->flags;

        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
        VK_CHECK(vmaCreateBuffer(GetAllocator().Get(), &bufferCreateInfo, &allocCreateInfo, &m_buffer, &m_allocation, &m_allocInfo));

        // vmaCreateBuffer automatically binds buffer and memory if VMA_ALLOCATION_CREATE_DONT_BIND_BIT is not provided

        const bool isBufferCreated = m_buffer != VK_NULL_HANDLE;
        const bool isMemoryAllocated = m_allocation != VK_NULL_HANDLE;
        VK_ASSERT(isBufferCreated);
        VK_ASSERT(isMemoryAllocated);

        const bool isCreated = isBufferCreated && isMemoryAllocated;
        VK_ASSERT(isCreated);

        if ((info.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0) {
            VkBufferDeviceAddressInfo addressInfo = {};
            addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            addressInfo.buffer = m_buffer;
            m_deviceAddress = vkGetBufferDeviceAddress(vkDevice, &addressInfo);
        }

        m_pDevice = info.pDevice;

        SetCreated(isCreated);

        return isCreated;
    }

    
    void Buffer::Destroy()
    {
        if (!IsCreated()) {
            return;
        }

        vmaDestroyBuffer(GetAllocator().Get(), m_buffer, m_allocation);
        m_allocation = VK_NULL_HANDLE;
        m_buffer = VK_NULL_HANDLE;

        m_allocInfo = {};

        m_deviceAddress = 0;

        m_pDevice = nullptr;
        m_state.reset();

        Object::Destroy();
    }


    void* Buffer::Map(VkDeviceSize offset, VkDeviceSize size)
    {
        VK_ASSERT(IsCreated());
        VK_ASSERT(!IsMapped());

        size = size == VK_WHOLE_SIZE ? GetSize() : size;
        VK_ASSERT(offset + size <= GetSize());

        void* pData = nullptr;
        VK_CHECK(vmaMapMemory(GetAllocator().Get(), m_allocation, &pData));

        m_state.set(BIT_IS_MAPPED, pData != nullptr);

        return (void*)((uint8_t*)(pData) + offset);
    }


    void Buffer::Unmap()
    {
        VK_ASSERT(IsCreated());
        VK_ASSERT(IsMapped());

        vmaUnmapMemory(GetAllocator().Get(), m_allocation);

        m_state.set(BIT_IS_MAPPED, false);
    }


    const char* Buffer::GetDebugName() const
    {
        return Object::GetDebugName("Buffer");
    }
}