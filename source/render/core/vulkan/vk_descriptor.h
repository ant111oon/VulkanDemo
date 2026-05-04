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


    class DescriptorSetLayout : public Handle<VkDescriptorSetLayout>
    {
    public:
        struct Descriptor
        {
            uint32_t         binding;
            VkDescriptorType type;
            uint32_t         count;
            VkDeviceSize     offset; // Byte offset inside descriptor set
        };

        using Base = Handle<VkDescriptorSetLayout>;

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

        bool HasDescriptor(uint32_t binding) const;
        size_t GetDescriptorsCount() const;

        Device& GetDevice() const;

        // Size of descriptor set in bytes
        VkDeviceSize GetSize() const;

        bool IsDescriptorBufferCompatible() const;

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
        
        std::vector<Descriptor> m_descriptors;
        VkDeviceSize m_size = 0;

        std::bitset<BIT_COUNT> m_state = {};
    };


    struct DescriptorBufferCreateInfo
    {
        Device* pDevice;
        std::span<DescriptorSetLayout*> layouts;
    };


    class DescriptorBuffer
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

        VkDeviceSize GetSetOffset(uint32_t index) const;
        const DescriptorSetLayout* GetDescriptorSetLayout(uint32_t index) const;

        template <typename... Args>
        DescriptorBuffer& SetDebugName(std::string_view fmt, Args&&... args)
        {
            GetDevice().SetObjDebugName(m_buffer, fmt, std::forward<Args>(args)...);
            return *this;
        }

        std::string_view GetDebugName() const;

        size_t GetSetCount() const;

        Device& GetDevice() const;

        const Buffer& Get() const;

        bool IsCreated() const;

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