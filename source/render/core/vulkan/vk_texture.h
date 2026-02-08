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
        ENG_DECL_CLASS_NO_COPIABLE(TextureView);

        TextureView() = default;
        TextureView(const TextureViewCreateInfo& info);

        ~TextureView();

        TextureView(TextureView&& view) noexcept;
        TextureView& operator=(TextureView&& view) noexcept;

        TextureView& Create(const TextureViewCreateInfo& info);
        TextureView& Create(const Texture& texture, const VkComponentMapping mapping, const VkImageSubresourceRange& subresourceRange);
        
        TextureView& Destroy();

        TextureView& SetDebugName(const char* pName);
        const char* GetDebugName() const;

        template <typename... Args>
        TextureView& SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(*GetDevice(), (uint64_t)m_view, VK_OBJECT_TYPE_IMAGE_VIEW, pFmt, std::forward<Args>(args)...);
            return *this;
        }

        const Texture* GetOwner() const
        {
            VK_ASSERT(IsCreated());
            return m_pOwner;
        }

        Device* GetDevice() const;

        VkImageView Get() const
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
        
        VkComponentMapping GetComponentMapping() const
        {
            VK_ASSERT(IsValid());
            return m_components;
        }

        VkImageSubresourceRange GetSubresoureRange() const
        {
            VK_ASSERT(IsValid());
            return m_subresourceRange;
        }

        bool IsValid() const;

    private:
        const Texture* m_pOwner = nullptr;

        VkImageView m_view = VK_NULL_HANDLE;

        VkImageViewType         m_type = {};
        VkFormat                m_format = {};
        VkComponentMapping      m_components = {};
        VkImageSubresourceRange m_subresourceRange = {};
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

        using AccessStateMipChain = std::vector<AccessState>;
        using AccessStateLayerMipChain = std::vector<AccessStateMipChain>;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Texture);

        Texture() = default;
        Texture(const TextureCreateInfo& info);

        ~Texture();

        Texture(Texture&& image) noexcept;
        Texture& operator=(Texture&& image) noexcept;

        Texture& Create(const TextureCreateInfo& info);
        Texture& Destroy();

        const char* GetDebugName() const
        {
            return Object::GetDebugName("Texture");
        }

        template <typename... Args>
        Texture& SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(*GetDevice(), (uint64_t)m_image, VK_OBJECT_TYPE_IMAGE, pFmt, std::forward<Args>(args)...);
            return *this;
        }

        Device* GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_pDevice;
        }

        VkImage Get() const
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
        Device* m_pDevice = nullptr;

        VkImage m_image = VK_NULL_HANDLE;
        
        VmaAllocation     m_allocation = VK_NULL_HANDLE;
        VmaAllocationInfo m_allocInfo = {};

        VkImageType m_type = {};
        VkExtent3D m_extent = {};
        VkFormat m_format = {};

        uint32_t m_mipCount = 1;
        uint32_t m_layersCount = 1;
 
        // one layer, one mip -> no dyn allocations
        // one layer, N mip -> one dyn allocation
        // N layer, M mip -> N + 1 dyn allocation
        std::variant<AccessState, AccessStateMipChain, AccessStateLayerMipChain> m_accessStates = AccessState{};
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
            Object::SetDebugName(*GetDevice(), (uint64_t)m_sampler, VK_OBJECT_TYPE_SAMPLER, pFmt, std::forward<Args>(args)...);
            return *this;
        }

        const char* GetDebugName() const;

        Device* GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_pDevice;
        }

        VkSampler Get() const
        {
            VK_ASSERT(IsCreated());
            return m_sampler;
        }

    private:
        Device* m_pDevice = nullptr;

        VkSampler m_sampler = VK_NULL_HANDLE;
    };
}