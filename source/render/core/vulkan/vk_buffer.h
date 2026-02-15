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
        VkBufferUsageFlags2 usage;

        const AllocationInfo* pAllocInfo;
    };


    class Buffer : public Object
    {
        friend class CmdBuffer;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Buffer);

        Buffer() = default;
        Buffer(Device* pDevice, VkDeviceSize size, VkBufferUsageFlags2 usage, const AllocationInfo& allocInfo);
        Buffer(const BufferCreateInfo& info);

        ~Buffer();

        Buffer(Buffer&& buffer) noexcept;
        Buffer& operator=(Buffer&& buffer) noexcept;

        Buffer& Create(Device* pDevice, VkDeviceSize size, VkBufferUsageFlags2 usage, const AllocationInfo& allocInfo);
        Buffer& Create(const BufferCreateInfo& info);
        Buffer& Destroy();

        void* Map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
        Buffer& Map(void** ppData, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

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

        const VkBuffer& Get() const
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

        bool IsPersistentlyMapped() const
        {
            VK_ASSERT(IsCreated());
            return m_state.test(BIT_IS_PERSISTANTLY_MAPPED);
        }

        bool IsUniformBuffer() const
        {
            VK_ASSERT(IsCreated());
            return m_state.test(BIT_IS_UNIFORM_BUFFER);
        }

        bool IsStorageBuffer() const
        {
            VK_ASSERT(IsCreated());
            return m_state.test(BIT_IS_STORAGE_BUFFER);
        }

        bool IsIndexBuffer() const
        {
            VK_ASSERT(IsCreated());
            return m_state.test(BIT_IS_INDEX_BUFFER);
        }

        bool IsDescriptorBuffer() const
        {
            VK_ASSERT(IsCreated());
            return m_state.test(BIT_IS_DESCRIPTOR_BUFFER);
        }

    private:
        void Transit(VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccessMask);

        VkPipelineStageFlags2 GetStageMask() const
        {
            VK_ASSERT(IsCreated());
            return m_currStageMask;
        }

        VkAccessFlags2 GetAccessMask() const
        {
            VK_ASSERT(IsCreated());
            return m_currAccessMask;
        }

    private:
        enum StateBits
        {
            BIT_IS_MAPPED,
            BIT_IS_PERSISTANTLY_MAPPED,
            BIT_IS_STORAGE_BUFFER,
            BIT_IS_UNIFORM_BUFFER,
            BIT_IS_INDEX_BUFFER,
            BIT_IS_DESCRIPTOR_BUFFER,
            BIT_COUNT,
        };

    private:
        Device* m_pDevice = nullptr;

        VkBuffer m_buffer = VK_NULL_HANDLE;

        VmaAllocation m_allocation = VK_NULL_HANDLE;
        VmaAllocationInfo m_allocInfo = {};

        VkDeviceAddress m_deviceAddress = 0;

        VkPipelineStageFlags2 m_currStageMask = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2        m_currAccessMask = VK_ACCESS_2_NONE;

        std::bitset<BIT_COUNT> m_state = {};
    };
}