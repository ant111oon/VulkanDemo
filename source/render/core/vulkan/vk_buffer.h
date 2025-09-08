#pragma once

#include "vk_core.h"

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


    class Buffer
    {
    public:
        Buffer() = default;
        Buffer(const BufferCreateInfo& info);

        Buffer(const Buffer& buffer) = delete;
        Buffer& operator=(const Buffer& buffer) = delete;

        Buffer(Buffer&& buffer) noexcept;
        Buffer& operator=(Buffer&& buffer) noexcept;

        bool Create(const BufferCreateInfo& info);
        void Destroy();

        void* Map(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags);
        void Unmap();

        void SetDebugName(const char* pName);
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

        bool IsMapped() const { return m_state.test(FLAG_IS_MAPPED); }
        bool IsCreated() const { return m_state.test(FLAG_IS_CREATED); }

    private:
        enum StateFlags
        {
            FLAG_IS_CREATED,
            FLAG_IS_MAPPED,
            FLAG_COUNT,
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

    #ifdef ENG_BUILD_DEBUG
        std::array<char, utils::MAX_VK_OBJ_DBG_NAME_LENGTH> m_debugName = {};
    #endif

        std::bitset<FLAG_COUNT> m_state = {};
    };
}