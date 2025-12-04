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

        Buffer& Create(const BufferCreateInfo& info);
        Buffer& Destroy();

        template<typename T>
        T* Map() { return static_cast<T*>(Map(0, VK_WHOLE_SIZE)); }

        template<typename T>
        Buffer& Map(T** ppData)
        {
            VK_ASSERT(ppData);
            *ppData = Map<T>();

            return *this;
        }

        void* Map(VkDeviceSize offset, VkDeviceSize size);
        Buffer& Map(void** ppData, VkDeviceSize offset, VkDeviceSize size);

        Buffer& Unmap();

        template <typename... Args>
        Buffer& SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(*m_pDevice, (uint64_t)m_buffer, VK_OBJECT_TYPE_BUFFER, pFmt, std::forward<Args>(args)...);
            return *this;
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

        VkDeviceSize GetMemorySize() const
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
        VmaAllocationInfo m_allocInfo = {};

        VkDeviceAddress m_deviceAddress = 0;

        std::bitset<BIT_COUNT> m_state = {};
    };
}