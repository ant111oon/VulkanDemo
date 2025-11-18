#pragma once

#include "vk_object.h"
#include "vk_device.h"
#include "vk_memory.h"


namespace vkn
{
    struct BufferCreateInfo
    {
        Device* pDevice;

        VkDeviceSize size;
        VkBufferUsageFlags usage;

        const AllocationInfo* pAllocInfo;
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

        template<typename T>
        T* Map()
        {
            return static_cast<T*>(Map(0, VK_WHOLE_SIZE));
        }

        void* Map(VkDeviceSize offset, VkDeviceSize size);
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
            return m_allocInfo.deviceMemory;
        }

        VkDeviceAddress GetDeviceAddress() const
        {
            VK_ASSERT(IsCreated());
            return m_deviceAddress;
        }

        VkDeviceSize GetSize() const
        {
            VK_ASSERT(IsCreated());
            return m_allocInfo.size;
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
        VmaAllocation m_allocation = VK_NULL_HANDLE;

        VkDeviceAddress m_deviceAddress = 0;
        
        VmaAllocationInfo m_allocInfo = {};

        std::bitset<BIT_COUNT> m_state = {};
    };
}