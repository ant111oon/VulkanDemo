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


    Buffer& Buffer::Create(const BufferCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of buffer %s", GetDebugName());
            Destroy();
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());
        VK_ASSERT(GetAllocator().IsCreated());
        VK_ASSERT(info.pAllocInfo);

        VkDevice vkDevice = info.pDevice->Get();

        VkBufferCreateInfo ci = {};
        ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        ci.size = info.size;
        ci.usage = info.usage;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // TODO: fix

        VmaAllocationCreateInfo allocCI = {};
        allocCI.usage = info.pAllocInfo->usage;
        allocCI.flags = info.pAllocInfo->flags;

        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
        VK_CHECK(vmaCreateBuffer(GetAllocator().Get(), &ci, &allocCI, &m_buffer, &m_allocation, &m_allocInfo));

        // vmaCreateBuffer automatically binds buffer and memory if VMA_ALLOCATION_CREATE_DONT_BIND_BIT is not provided

        VK_ASSERT_MSG(m_buffer != VK_NULL_HANDLE, "Failed to create Vulkan buffer");
        VK_ASSERT_MSG(m_allocation != VK_NULL_HANDLE, "Failed to allocate Vulkan texture memory");

        if ((info.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0) {
            VkBufferDeviceAddressInfo addressInfo = {};
            addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            addressInfo.buffer = m_buffer;
            m_deviceAddress = vkGetBufferDeviceAddress(vkDevice, &addressInfo);
        }

        m_pDevice = info.pDevice;

        SetCreated(true);

        return *this;
    }

    
    Buffer& Buffer::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        vmaDestroyBuffer(GetAllocator().Get(), m_buffer, m_allocation);
        m_allocation = VK_NULL_HANDLE;
        m_buffer = VK_NULL_HANDLE;

        m_allocInfo = {};

        m_deviceAddress = 0;

        m_pDevice = nullptr;
        m_state.reset();

        Object::Destroy();

        return *this;
    }


    void* Buffer::Map(VkDeviceSize offset, VkDeviceSize size)
    {
        VK_ASSERT(IsCreated());
        VK_ASSERT(!IsMapped());

        size = size == VK_WHOLE_SIZE ? GetMemorySize() : size;
        VK_ASSERT(offset + size <= GetMemorySize());

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
        VK_ASSERT(IsMapped());

        vmaUnmapMemory(GetAllocator().Get(), m_allocation);

        m_state.set(BIT_IS_MAPPED, false);

        return *this;
    }


    const char* Buffer::GetDebugName() const
    {
        return Object::GetDebugName("Buffer");
    }
}