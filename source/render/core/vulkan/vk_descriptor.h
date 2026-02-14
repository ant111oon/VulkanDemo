#pragma once

#include "vk_buffer.h"

#include <span>


namespace vkn
{
    struct DescriptorInfo
    {
        static constexpr DescriptorInfo Create(uint32_t binding, VkDescriptorType type, uint32_t count, VkShaderStageFlags stagesMask, VkDescriptorBindingFlags flags = 0)
        {
            return DescriptorInfo { binding, type, count, stagesMask, flags };
        }

        uint32_t                 binding;
        VkDescriptorType         type;
        uint32_t                 count;
        VkShaderStageFlags       stagesMask;
        VkDescriptorBindingFlags flags;
    };


    struct DescriptorSetLayoutCreateInfo
    {
        Device* pDevice;

        VkDescriptorSetLayoutCreateFlags flags;
        std::span<const DescriptorInfo> descriptorInfos;
    };


    class DescriptorSetLayout : public Object
    {
    public:
        ENG_DECL_CLASS_NO_COPIABLE(DescriptorSetLayout);

        DescriptorSetLayout() = default;
        DescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info);
        DescriptorSetLayout(Device* pDevice, VkDescriptorSetLayoutCreateFlags flags, std::span<const DescriptorInfo> descriptorInfos);
        
        ~DescriptorSetLayout();

        DescriptorSetLayout(DescriptorSetLayout&& layout) noexcept;
        DescriptorSetLayout& operator=(DescriptorSetLayout&& layout) noexcept;

        DescriptorSetLayout& Create(const DescriptorSetLayoutCreateInfo& info);
        DescriptorSetLayout& Create(Device* pDevice, VkDescriptorSetLayoutCreateFlags flags, std::span<const DescriptorInfo> descriptorInfos);
        DescriptorSetLayout& Destroy();

        const char* GetDebugName() const
        {
            return Object::GetDebugName("DescriptorSetLayout");
        }

        template <typename... Args>
        DescriptorSetLayout& SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(*GetDevice(), (uint64_t)m_layout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, pFmt, std::forward<Args>(args)...);
            return *this;
        }

        Device* GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_pDevice;
        }

        VkDescriptorSetLayout Get() const
        {
            VK_ASSERT(IsCreated());
            return m_layout;
        }

        VkDeviceSize GetSize() const
        {
            VK_ASSERT(IsCreated());
            VK_ASSERT(IsDescriptorBufferCompatible());

            return m_size;
        }

        bool IsDescriptorBufferCompatible() const
        {
            VK_ASSERT(IsCreated());
            return m_state.test(BIT_IS_DESCRIPTOR_BUFFER_SOMPATIBLE);
        }

    private:
        enum StateBits
        {
            BIT_IS_DESCRIPTOR_BUFFER_SOMPATIBLE,
            BIT_COUNT,
        };

    private:        
        Device* m_pDevice = nullptr;

        VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
        
        std::vector<VkDeviceSize> m_descriptorOffsets;
        VkDeviceSize m_size = 0;

        std::bitset<BIT_COUNT> m_state = {};
    };
}