#pragma once

#include "vk_buffer.h"

#include <span>


namespace vkn
{
    class TextureView;
    class Sampler;


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
        struct Descriptor
        {
            uint32_t         binding;
            VkDescriptorType type;
            uint32_t         count;
            VkDeviceSize     offset; // Byte offset inside descriptor set
        };

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

        Descriptor& GetDescriptorByIdx(uint32_t index);
        const Descriptor& GetDescriptorByIdx(uint32_t index) const;

        Descriptor& GetDescriptorByBinding(uint32_t binding);
        const Descriptor& GetDescriptorByBinding(uint32_t binding) const;

        bool HasDescriptor(uint32_t binding) const
        {
            VK_ASSERT(IsCreated());
            return GetDescriptorIndex(binding) != UINT64_MAX;
        }

        size_t GetDescriptorsCount() const
        {
            VK_ASSERT(IsCreated());
            return m_descriptors.size();
        }

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

        const VkDescriptorSetLayout& Get() const
        {
            VK_ASSERT(IsCreated());
            return m_layout;
        }

        // Size of descriptor set in bytes
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
        uint64_t GetDescriptorIndex(uint32_t binding) const;

    private:
        enum StateBits
        {
            BIT_IS_DESCRIPTOR_BUFFER_SOMPATIBLE,
            BIT_COUNT,
        };

    private:        
        Device* m_pDevice = nullptr;

        VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
        
        std::vector<Descriptor> m_descriptors;
        VkDeviceSize m_size = 0;

        std::bitset<BIT_COUNT> m_state = {};
    };


    struct DescriptorBufferCreateInfo
    {
        Device* pDevice;
        std::span<DescriptorSetLayout*> layouts;
    };


    class DescriptorBuffer : public Object
    {
    public:
        ENG_DECL_CLASS_NO_COPIABLE(DescriptorBuffer);

        DescriptorBuffer() = default;
        DescriptorBuffer(Device* pDevice, std::span<DescriptorSetLayout*> layouts);
        DescriptorBuffer(const DescriptorBufferCreateInfo& info);

        ~DescriptorBuffer();

        DescriptorBuffer(DescriptorBuffer&& buffer) noexcept;
        DescriptorBuffer& operator=(DescriptorBuffer&& buffer) noexcept;

        DescriptorBuffer& Create(Device* pDevice, std::span<DescriptorSetLayout*> layouts);
        DescriptorBuffer& Create(const DescriptorBufferCreateInfo& info);
        DescriptorBuffer& Destroy();

        DescriptorBuffer& WriteDescriptor(uint32_t setIdx, uint32_t binding, uint32_t elemIdx, const Buffer& buffer);
        DescriptorBuffer& WriteDescriptor(uint32_t setIdx, uint32_t binding, uint32_t elemIdx, const TextureView& texture);
        DescriptorBuffer& WriteDescriptor(uint32_t setIdx, uint32_t binding, uint32_t elemIdx, const Sampler& sampler);

        const char* GetDebugName() const
        {
            return m_buffer.GetDebugName();
        }

        template <typename... Args>
        DescriptorBuffer& SetDebugName(const char* pFmt, Args&&... args)
        {
            m_buffer.SetDebugName(pFmt, std::forward<Args>(args)...);
            return *this;
        }

        Device* GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_buffer.GetDevice();
        }

        const Buffer& Get() const
        {
            VK_ASSERT(IsCreated());
            return m_buffer;
        }

    private:
        struct Entry
        {
            VkDeviceSize         offset;
            DescriptorSetLayout* pLayout;
        }; 

    private:
        Buffer m_buffer;
        std::vector<Entry> m_entries;
    };
}