#pragma once

#include "vk_object.h"
#include "vk_device.h"
#include "vk_memory.h"


namespace vkn
{
    class Texture;


    struct TextureViewCreateInfo
    {
        const Texture*          pOwner;

        VkImageViewType         type;
        VkFormat                format;
        VkComponentMapping      components;
        VkImageSubresourceRange subresourceRange;
    };


    class TextureView : public Object
    {
    public:
        struct SubresourceRange
        {
            uint8_t baseMipLevel = 0;
            uint8_t levelCount = 0;
            uint8_t baseArrayLayer = 0;
            uint8_t layerCount = 0;
        };

    public:
        ENG_DECL_CLASS_NO_COPIABLE(TextureView);

        TextureView() = default;
        TextureView(const TextureViewCreateInfo& info);

        ~TextureView();

        TextureView(TextureView&& view) noexcept;
        TextureView& operator=(TextureView&& view) noexcept;

        TextureView& Create(const TextureViewCreateInfo& info);
        TextureView& Create(const Texture& texture, const VkComponentMapping mapping, const VkImageSubresourceRange& subresourceRange);
        
        TextureView& Destroy();

        bool CheckLayoutConsistency() const;

        TextureView& SetDebugName(const char* pName);
        const char* GetDebugName() const;

        template <typename... Args>
        TextureView& SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(GetDevice(), (uint64_t)m_view, VK_OBJECT_TYPE_IMAGE_VIEW, pFmt, std::forward<Args>(args)...);
            return *this;
        }

        const Texture& GetOwner() const
        {
            VK_ASSERT(IsCreated());
            return *m_pOwner;
        }

        Device& GetDevice() const;

        const VkImageView& Get() const
        {
            VK_ASSERT(IsValid());
            return m_view;
        }

        VkImageViewType GetType() const
        {
            VK_ASSERT(IsValid());
            return m_type;
        }

        VkFormat GetFormat() const
        {
            VK_ASSERT(IsValid());
            return m_format;
        }

        const SubresourceRange& GetSubresourceRange() const
        {
            VK_ASSERT(IsValid());
            return m_subresRange;
        }

        uint32_t GetViewBaseMip() const { return GetSubresourceRange().baseMipLevel; }
        uint32_t GetViewMipCount() const { return GetSubresourceRange().levelCount; }
        uint32_t GetViewBaseArrayLayer() const { return GetSubresourceRange().baseArrayLayer; }
        uint32_t GetViewLayerCount() const { return GetSubresourceRange().layerCount; }

        bool IsValid() const;

    private:
        const Texture* m_pOwner = nullptr;

        VkImageView m_view = VK_NULL_HANDLE;

        VkImageViewType m_type = {};
        VkFormat m_format = {};
        SubresourceRange m_subresRange = {};
    };


    struct TextureCreateInfo
    {
        Device* pDevice;

        VkImageType           type;
        VkExtent3D            extent;
        VkFormat              format;
        VkImageUsageFlags     usage; 
        VkImageLayout         initialLayout;
        VkImageCreateFlags    flags;
        uint32_t              mipLevels;
        uint32_t              arrayLayers;
        VkSampleCountFlagBits samples;
        VkImageTiling         tiling;
        
        const AllocationInfo* pAllocInfo;
    };


    class Texture : public Object
    {
        friend class CmdBuffer;
        friend class DescriptorBuffer;

        struct AccessState;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Texture);

        Texture() = default;
        Texture(const TextureCreateInfo& info);

        ~Texture();

        Texture(Texture&& image) noexcept;
        Texture& operator=(Texture&& image) noexcept;

        Texture& Create(const TextureCreateInfo& info);
        Texture& Destroy();

        // Check if mips from baseMip to baseMip + mipCount - 1 in layers from baseLayer to baseLayer + layerCount - 1 have same layout
        bool CheckLayoutConsistency(uint32_t baseLayer, uint32_t layerCount, uint32_t baseMip, uint32_t mipCount) const;

        const char* GetDebugName() const
        {
            return Object::GetDebugName("Texture");
        }

        template <typename... Args>
        Texture& SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(GetDevice(), (uint64_t)m_image, VK_OBJECT_TYPE_IMAGE, pFmt, std::forward<Args>(args)...);
            return *this;
        }

        Device& GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return *m_pDevice;
        }

        const VkImage& Get() const
        {
            VK_ASSERT(IsCreated());
            return m_image;
        }

        VkDeviceMemory GetMemory() const
        {
            VK_ASSERT(IsCreated());
            return m_allocInfo.deviceMemory;
        }

        VkDeviceSize GetMemorySize() const
        {
            VK_ASSERT(IsCreated());
            return m_allocInfo.size;
        }

        VkImageType GetType() const
        {
            VK_ASSERT(IsCreated());
            return m_type;
        }

        VkFormat GetFormat() const
        {
            VK_ASSERT(IsCreated());
            return m_format;    
        }

        const VkExtent3D& GetSize() const
        {
            VK_ASSERT(IsCreated());
            return m_extent;    
        }

        uint32_t GetMipCount() const
        {
            VK_ASSERT(IsCreated());
            return m_mipCount;
        }

        uint32_t GetLayerCount() const
        {
            VK_ASSERT(IsCreated());
            return m_layersCount;
        }

        uint32_t GetSizeX() const { return GetSize().width; }
        uint32_t GetSizeY() const { return GetSize().height; }
        uint32_t GetSizeZ() const { return GetSize().depth; }

    private:
        void Transit(uint32_t baseMip, uint32_t mipCount, uint32_t baseLayer, uint32_t layerCount,
            VkImageLayout dstLayout, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask);

        void InitAccessStates(const TextureCreateInfo& info);

        const AccessState& GetAccessState(uint32_t layer, uint32_t mip) const;

    private:
        struct AccessState
        {
            bool operator==(const AccessState& state) const
            {
                return layout == state.layout && stageMask == state.stageMask && accessMask == state.accessMask;
            }

            VkImageLayout         layout = VK_IMAGE_LAYOUT_UNDEFINED; 
            VkPipelineStageFlags2 stageMask = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2        accessMask = VK_ACCESS_2_NONE;
        };

        using AccessStateArray = std::vector<AccessState>;

    private:
        Device* m_pDevice = nullptr;

        VkImage m_image = VK_NULL_HANDLE;
        
        VmaAllocation     m_allocation = VK_NULL_HANDLE;
        VmaAllocationInfo m_allocInfo = {};

        VkImageType m_type = {};
        VkExtent3D m_extent = {};
        VkFormat m_format = {};

        uint32_t m_mipCount = 1;
        uint32_t m_layersCount = 1;
 
        std::variant<AccessState, AccessStateArray> m_accessStates = AccessState{};
    };


    struct SamplerCreateInfo
    {
        Device* pDevice;

        VkSamplerCreateFlags    flags;
        VkFilter                magFilter;
        VkFilter                minFilter;
        VkSamplerMipmapMode     mipmapMode;
        VkSamplerAddressMode    addressModeU;
        VkSamplerAddressMode    addressModeV;
        VkSamplerAddressMode    addressModeW;
        float                   mipLodBias;
        VkBool32                anisotropyEnable;
        float                   maxAnisotropy;
        VkBool32                compareEnable;
        VkCompareOp             compareOp;
        float                   minLod;
        float                   maxLod;
        VkBorderColor           borderColor;
        VkBool32                unnormalizedCoordinates;
    };


    class Sampler : public Object
    {
    public:
        ENG_DECL_CLASS_NO_COPIABLE(Sampler);

        Sampler() = default;
        Sampler(const SamplerCreateInfo& info);

        ~Sampler();

        Sampler(Sampler&& sampler) noexcept;
        Sampler& operator=(Sampler&& sampler) noexcept;

        Sampler& Create(const SamplerCreateInfo& info);
        Sampler& Destroy();

        template <typename... Args>
        Sampler& SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(GetDevice(), (uint64_t)m_sampler, VK_OBJECT_TYPE_SAMPLER, pFmt, std::forward<Args>(args)...);
            return *this;
        }

        const char* GetDebugName() const;

        Device& GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return *m_pDevice;
        }

        const VkSampler& Get() const
        {
            VK_ASSERT(IsCreated());
            return m_sampler;
        }

    private:
        Device* m_pDevice = nullptr;

        VkSampler m_sampler = VK_NULL_HANDLE;
    };
}