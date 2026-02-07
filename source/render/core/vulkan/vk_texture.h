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

        const uint32_t GetMipCount() const
        {
            VK_ASSERT(IsCreated());
            return m_mipCount;
        }

        const uint32_t GetLayersCount() const
        {
            VK_ASSERT(IsCreated());
            return m_layersCount;
        }

        const uint32_t GetSizeX() const { return GetSize().width; }
        const uint32_t GetSizeY() const { return GetSize().height; }
        const uint32_t GetSizeZ() const { return GetSize().depth; }

    private:
        void Transit(VkImageLayout dstLayout, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask);

        VkImageLayout GetLayout() const
        {
            VK_ASSERT(IsCreated());
            return m_currLayout;
        }

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
        Device* m_pDevice = nullptr;

        VkImage m_image = VK_NULL_HANDLE;
        
        VmaAllocation     m_allocation = VK_NULL_HANDLE;
        VmaAllocationInfo m_allocInfo = {};

        VkImageType m_type = {};
        VkExtent3D m_extent = {};
        VkFormat m_format = {};

        uint32_t m_mipCount = 1;
        uint32_t m_layersCount = 1;
 
        VkImageLayout         m_currLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
        VkPipelineStageFlags2 m_currStageMask = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2        m_currAccessMask = VK_ACCESS_2_NONE;
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