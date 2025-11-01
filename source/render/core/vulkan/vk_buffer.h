#pragma once

#include "vk_object.h"
#include "vk_device.h"


namespace vkn
{
    struct BufferCreateInfo
    {
        Device* pDevice;

        VkDeviceSize size;
        VkBufferUsageFlags usage;
        VkMemoryPropertyFlags properties;
        VkMemoryAllocateFlags memAllocFlags;
    };


    class Buffer : public Object
    {
    public:
        ENG_DECL_CLASS_NO_COPIABLE(Buffer);

        Buffer() = default;
        Buffer(const BufferCreateInfo& info);

        ~Buffer();

        Buffer(Buffer&& buffer) noexcept;
        Buffer& operator=(Buffer&& buffer) noexcept;

        bool Create(const BufferCreateInfo& info);
        void Destroy();

        void* Map(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags);
        void Unmap();

        template <typename... Args>
        void SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(*m_pDevice, (uint64_t)m_buffer, VK_OBJECT_TYPE_BUFFER, pFmt, std::forward<Args>(args)...);
        }

        const char* GetDebugName() const;

        Device* GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_pDevice;
        }

        VkBuffer Get() const
        {
            VK_ASSERT(IsCreated());
            return m_buffer;
        }

        VkDeviceMemory GetMemory() const
        {
            VK_ASSERT(IsCreated());
            return m_memory;
        }

        VkDeviceAddress GetDeviceAddress() const
        {
            VK_ASSERT(IsCreated());
            return m_deviceAddress;
        }

        VkDeviceSize GetSize() const
        {
            VK_ASSERT(IsCreated());
            return m_size;
        }

        bool TestProperties(VkMemoryPropertyFlags properties)
        {
            VK_ASSERT(IsCreated());
            return (m_properties & properties) == properties;
        }

        bool TestMemoryAllocFlags(VkMemoryAllocateFlags memoryAllocFlags)
        {
            VK_ASSERT(IsCreated());
            return (m_memAllocFlags & memoryAllocFlags) == memoryAllocFlags;
        }

        bool IsMapped() const
        {
            VK_ASSERT(IsCreated());
            return m_state.test(BIT_IS_MAPPED);
        }

    private:
        enum StateBits
        {
            BIT_IS_MAPPED,
            BIT_COUNT,
        };

    private:
        Device* m_pDevice = nullptr;

        VkBuffer m_buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_memory = VK_NULL_HANDLE;
        VkDeviceAddress m_deviceAddress = 0;

        VkDeviceSize m_size = 0;
        VkBufferUsageFlags m_usage = {};
        VkMemoryPropertyFlags m_properties = {};
        VkMemoryAllocateFlags m_memAllocFlags = {};

        std::bitset<BIT_COUNT> m_state = {};
    };
}