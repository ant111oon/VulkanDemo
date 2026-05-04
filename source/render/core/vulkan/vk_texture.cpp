#include "pch.h"

#include "vk_Texture.h"
#include "vk_utils.h"


namespace vkn
{
    Device& TextureView::GetDevice() const
    {
        VK_ASSERT(IsValid());
        return m_pOwner->GetDevice();
    }


    TextureView::TextureView(const TextureViewCreateInfo& info)
    {
        Create(info);
    }


    TextureView::~TextureView()
    {
        Destroy();
    }


    TextureView::TextureView(TextureView&& view) noexcept
    {
        *this = std::move(view);
    }


    TextureView& TextureView::operator=(TextureView&& view) noexcept
    {
        if (this == &view) {
            return *this;
        }

        if (IsCreated()) {
            Destroy();
        }

        std::swap(m_pOwner, view.m_pOwner);
        std::swap(m_type, view.m_type);
        std::swap(m_format, view.m_format);
        std::swap(m_subresRange, view.m_subresRange);

        Base::operator=(std::move(view));

        return *this;
    }


    TextureView& TextureView::Create(const TextureViewCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of texture view %s", GetDebugName().data());
            Destroy();
        }

        const Texture* pOwner = info.pOwner;

        VK_ASSERT(pOwner && pOwner->IsCreated());

        VkImageSubresourceRange subresourceRange = info.subresourceRange;

        if (subresourceRange.levelCount == VK_REMAINING_MIP_LEVELS) {
            subresourceRange.levelCount = pOwner->GetMipCount();    
        }

        if (subresourceRange.layerCount == VK_REMAINING_ARRAY_LAYERS) {
            subresourceRange.layerCount = pOwner->GetLayerCount();    
        }

        VK_ASSERT(subresourceRange.baseMipLevel < std::numeric_limits<decltype(m_subresRange.baseMipLevel)>::max());
        VK_ASSERT(subresourceRange.levelCount < std::numeric_limits<decltype(m_subresRange.levelCount)>::max());
        VK_ASSERT(subresourceRange.baseArrayLayer < std::numeric_limits<decltype(m_subresRange.baseArrayLayer)>::max());
        VK_ASSERT(subresourceRange.layerCount < std::numeric_limits<decltype(m_subresRange.layerCount)>::max());

        VkImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = pOwner->Get();
        imageViewCreateInfo.viewType = info.type;
        imageViewCreateInfo.format = info.format;
        imageViewCreateInfo.components = info.components;
        imageViewCreateInfo.subresourceRange = subresourceRange;

        Base::Create([vkDevice = pOwner->GetDevice().Get(), &imageViewCreateInfo](VkImageView& view) {
            VK_CHECK(vkCreateImageView(vkDevice, &imageViewCreateInfo, nullptr, &view));
            return view != VK_NULL_HANDLE;
        });

        VK_ASSERT(IsCreated());

        m_pOwner = pOwner;

        m_type = info.type;
        m_format = info.format;
        m_subresRange.baseMipLevel = subresourceRange.baseMipLevel;
        m_subresRange.levelCount = subresourceRange.levelCount;
        m_subresRange.baseArrayLayer = subresourceRange.baseArrayLayer;
        m_subresRange.layerCount = subresourceRange.layerCount;

        return *this;
    }


    TextureView& TextureView::Create(const Texture& texture, const VkComponentMapping mapping, const VkImageSubresourceRange& subresourceRange)
    {
        VK_ASSERT(texture.IsCreated());

        TextureViewCreateInfo createInfo = {};
        createInfo.pOwner = &texture;
        createInfo.type = utils::ImageTypeToViewType(texture.GetType());
        createInfo.format = texture.GetFormat();
        createInfo.components = mapping;
        createInfo.subresourceRange = subresourceRange;

        return Create(createInfo);
    }


    TextureView& TextureView::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        Base::Destroy([vkDevice = GetDevice().Get()](VkImageView& view) {
            vkDestroyImageView(vkDevice, view, nullptr);
        });

        m_pOwner = nullptr;
        m_type = {};
        m_format = {};
        m_subresRange = {};

        return *this;
    }


    bool TextureView::CheckLayoutConsistency() const
    {
        const SubresourceRange& range = GetSubresourceRange();
        return GetOwner().CheckLayoutConsistency(range.baseArrayLayer, range.layerCount, range.baseMipLevel, range.levelCount);
    }


    const Texture& TextureView::GetOwner() const
    {
        VK_ASSERT(IsCreated());
        return *m_pOwner;
    }


    VkImageViewType TextureView::GetType() const
    {
        VK_ASSERT(IsValid());
        return m_type;
    }


    VkFormat TextureView::GetFormat() const
    {
        VK_ASSERT(IsValid());
        return m_format;
    }


    const TextureView::SubresourceRange& TextureView::GetSubresourceRange() const
    {
        VK_ASSERT(IsValid());
        return m_subresRange;
    }


    uint32_t TextureView::GetViewBaseMip() const
    {
        return GetSubresourceRange().baseMipLevel; 
    }


    uint32_t TextureView::GetViewMipCount() const
    {
        return GetSubresourceRange().levelCount; 
    }


    uint32_t TextureView::GetViewBaseArrayLayer() const
    {
        return GetSubresourceRange().baseArrayLayer; 
    }


    uint32_t TextureView::GetViewLayerCount() const
    {
        return GetSubresourceRange().layerCount; 
    }


    bool TextureView::IsValid() const
    {
        return IsCreated() && m_pOwner->IsCreated();
    }


    Texture::Texture(const TextureCreateInfo& info)
    {
        Create(info);
    }


    Texture::Texture(Texture&& image) noexcept
    {
        *this = std::move(image);
    }


    Texture::~Texture()
    {
        Destroy();
    }


    Texture& Texture::operator=(Texture&& image) noexcept
    {
        if (this == &image) {
            return *this;
        }

        if (IsCreated()) {
            Destroy();
        }
        
        std::swap(m_allocation, image.m_allocation);
        std::swap(m_allocInfo, image.m_allocInfo);
        
        std::swap(m_type, image.m_type);
        std::swap(m_extent, image.m_extent);
        std::swap(m_format, image.m_format);
        std::swap(m_mipCount, image.m_mipCount);
        std::swap(m_layersCount, image.m_layersCount);

        std::swap(m_accessStates, image.m_accessStates);

        std::swap(m_pDevice, image.m_pDevice);

        Base::operator=(std::move(image));

        return *this;
    }


    Texture& Texture::Create(const TextureCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of texture %s", GetDebugName().data());
            Destroy();
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());
        VK_ASSERT(info.pAllocInfo);
        VK_ASSERT(info.mipLevels >= 1);
        VK_ASSERT(info.arrayLayers >= 1);
        VK_ASSERT(GetAllocator().IsCreated());

        VkDevice vkDevice = info.pDevice->Get();

        VkImageCreateInfo ci = {};
        ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.flags = info.flags;
        ci.imageType = info.type;
        ci.format = info.format;
        ci.extent = info.extent;
        ci.mipLevels = info.mipLevels;
        ci.arrayLayers = info.arrayLayers;
        ci.samples = info.samples;
        ci.tiling = info.tiling;
        ci.usage = info.usage;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.queueFamilyIndexCount = 0;
        ci.pQueueFamilyIndices = nullptr;
        ci.initialLayout = info.initialLayout;

        VmaAllocationCreateInfo allocCI = {};
        allocCI.usage = info.pAllocInfo->usage;
        allocCI.flags = info.pAllocInfo->flags;

        Base::Create([&ci, &allocCI, &allocation = m_allocation, &allocInfo = m_allocInfo](VkImage& dstImage) {
            VK_CHECK(vmaCreateImage(GetAllocator().Get(), &ci, &allocCI, &dstImage, &allocation, &allocInfo));
            return dstImage != VK_NULL_HANDLE;
        });
        
        VK_ASSERT_MSG(IsCreated(), "Failed to create Vulkan texture");
        VK_ASSERT_MSG(m_allocation != VK_NULL_HANDLE, "Failed to allocate Vulkan texture memory");

        m_pDevice = info.pDevice;

        m_type = info.type;
        m_extent = info.extent;
        m_format = info.format;
        m_mipCount = info.mipLevels;
        m_layersCount = info.arrayLayers;

        InitAccessStates(info);

        return *this;
    }


    Texture& Texture::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        Base::Destroy([&allocation = m_allocation](VkImage& image) {
            vmaDestroyImage(GetAllocator().Get(), image, allocation);
            allocation = VK_NULL_HANDLE;
        });

        m_allocInfo = {};

        m_pDevice = nullptr;

        m_type = {};
        m_extent = {};
        m_format = {};
        m_mipCount = 1;
        m_layersCount = 1;

        m_accessStates = AccessState{};

        return *this;
    }


    bool Texture::CheckLayoutConsistency(uint32_t baseLayer, uint32_t layerCount, uint32_t baseMip, uint32_t mipCount) const
    {
    #ifdef ENG_BUILD_DEBUG
        const uint32_t lastLayerIdx = baseLayer + layerCount - 1;
        const uint32_t lastMipIdx = baseMip + mipCount - 1;

        const VkImageLayout layout = GetAccessState(baseLayer, baseMip).layout;

        for (uint32_t layerIdx = baseLayer; layerIdx <= lastLayerIdx; ++layerIdx) {
            for (uint32_t mipIdx = baseMip; mipIdx <= lastMipIdx; ++mipIdx) {
                if (layout != GetAccessState(layerIdx, mipIdx).layout) {
                    VK_ASSERT_FAIL( 
                        "Texture %s subresource range (baseLayer: %u, layerCount: %u, baseMip: %u, mipCount: %u) has inconsistent layout", 
                        GetDebugName().data(), baseLayer, layerCount, baseMip, mipCount
                    );
                    
                    return false;
                }
            }
        }
    #endif

        return true;
    }


    Device& Texture::GetDevice() const
    {
        VK_ASSERT(IsCreated());
        return *m_pDevice;
    }


    VkDeviceMemory Texture::GetMemory() const
    {
        VK_ASSERT(IsCreated());
        return m_allocInfo.deviceMemory;
    }


    VkDeviceSize Texture::GetMemorySize() const
    {
        VK_ASSERT(IsCreated());
        return m_allocInfo.size;
    }


    VkImageType Texture::GetType() const
    {
        VK_ASSERT(IsCreated());
        return m_type;
    }


    VkFormat Texture::GetFormat() const
    {
        VK_ASSERT(IsCreated());
        return m_format;    
    }


    const VkExtent3D& Texture::GetSize() const
    {
        VK_ASSERT(IsCreated());
        return m_extent;    
    }


    uint32_t Texture::GetMipCount() const
    {
        VK_ASSERT(IsCreated());
        return m_mipCount;
    }


    uint32_t Texture::GetLayerCount() const
    {
        VK_ASSERT(IsCreated());
        return m_layersCount;
    }


    uint32_t Texture::GetSizeX() const
    {
        return GetSize().width;
    }


    uint32_t Texture::GetSizeY() const
    {
        return GetSize().height;
    }


    uint32_t Texture::GetSizeZ() const
    {
        return GetSize().depth;
    }


    void Texture::Transit(uint32_t baseMip, uint32_t mipCount, uint32_t baseLayer, uint32_t layerCount,
            VkImageLayout dstLayout, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask
    ) {
        VK_ASSERT(IsCreated());
        VK_ASSERT(mipCount >= 1);
        VK_ASSERT(layerCount >= 1);
        VK_ASSERT(baseMip + mipCount <= m_mipCount);
        VK_ASSERT(baseLayer + layerCount <= m_layersCount);

        auto FillAccessState = [](AccessState& outState, VkImageLayout dstLayout, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask)
        {
            outState.layout = dstLayout;
            outState.stageMask = dstStageMask;
            outState.accessMask = dstAccessMask;
        };

        if (std::holds_alternative<AccessState>(m_accessStates)) {
            AccessState& state = std::get<AccessState>(m_accessStates);
            FillAccessState(state, dstLayout, dstStageMask, dstAccessMask);
        } else {
            AccessStateArray& states = std::get<AccessStateArray>(m_accessStates);

            for (uint32_t i = 0; i < layerCount; ++i) {
                const uint32_t layer = baseLayer + i;

                for (uint32_t j = 0; j < mipCount; ++j) {
                    const uint32_t mip = baseMip + j;
                    const uint32_t index = layer * m_mipCount + mip;

                    FillAccessState(states[index], dstLayout, dstStageMask, dstAccessMask);
                }
            }
        }
    }


    void Texture::InitAccessStates(const TextureCreateInfo& info)
    {
        if (info.arrayLayers == 1 && info.mipLevels == 1) {
            m_accessStates = AccessState { info.initialLayout, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE };
        } else {
            m_accessStates = AccessStateArray(info.arrayLayers * info.mipLevels);
        }
    }


    const Texture::AccessState& Texture::GetAccessState(uint32_t layer, uint32_t mip) const
    {
        VK_ASSERT(IsCreated());

        VK_ASSERT(mip < m_mipCount);
        VK_ASSERT(layer < m_layersCount);

        if (std::holds_alternative<AccessState>(m_accessStates)) {
            return std::get<AccessState>(m_accessStates);
        } else {
            return std::get<AccessStateArray>(m_accessStates)[layer * m_mipCount + mip];
        }
    }


    Sampler::Sampler(const SamplerCreateInfo& info)
    {
        Create(info);
    }


    Sampler::Sampler(Sampler&& sampler) noexcept
    {
        *this = std::move(sampler);
    }


    Sampler::~Sampler()
    {
        Destroy();
    }


    Sampler& Sampler::operator=(Sampler&& sampler) noexcept
    {
        if (this == &sampler) {
            return *this;
        }

        if (IsCreated()) {
            Destroy();
        }
        
        std::swap(m_pDevice, sampler.m_pDevice);

        Base::operator=(std::move(sampler));

        return *this;
    }


    Sampler& Sampler::Create(const SamplerCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of sampler %s", GetDebugName().data());
            Destroy();
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());

        VkSamplerCreateInfo createInfo = {};

        createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        createInfo.magFilter = info.magFilter;
        createInfo.minFilter = info.minFilter;
        createInfo.mipmapMode = info.mipmapMode;
        createInfo.addressModeU = info.addressModeU;
        createInfo.addressModeV = info.addressModeV;
        createInfo.addressModeW = info.addressModeW;
        createInfo.mipLodBias = info.mipLodBias;
        createInfo.anisotropyEnable = info.anisotropyEnable;
        createInfo.maxAnisotropy = info.maxAnisotropy;
        createInfo.compareEnable = info.compareEnable;
        createInfo.compareOp = info.compareOp;
        createInfo.minLod = info.minLod;
        createInfo.maxLod = info.maxLod;
        createInfo.borderColor = info.borderColor;
        createInfo.unnormalizedCoordinates = info.unnormalizedCoordinates;

        Base::Create([vkDevice = info.pDevice->Get(), &createInfo](VkSampler& sampler) {
            VK_CHECK(vkCreateSampler(vkDevice, &createInfo, nullptr, &sampler));
            return sampler != VK_NULL_HANDLE;
        });

        VK_ASSERT_MSG(IsCreated(), "Failed to create Vulkan sampler");

        m_pDevice = info.pDevice;

        return *this;
    }


    Sampler& Sampler::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        Base::Destroy([vkDevice = m_pDevice->Get()](VkSampler& sampler) {
            vkDestroySampler(vkDevice, sampler, nullptr);
        });

        m_pDevice = nullptr;

        return *this;
    }


    Device& Sampler::GetDevice() const
    {
        VK_ASSERT(IsCreated());
        return *m_pDevice;
    }
}
