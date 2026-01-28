#include "pch.h"

#include "vk_Texture.h"
#include "vk_utils.h"


namespace vkn
{
    static constexpr VkImageViewType ImageTypeToViewType(VkImageType type)
    {
        switch(type) {
            case VK_IMAGE_TYPE_1D:
                return VK_IMAGE_VIEW_TYPE_1D;
            case VK_IMAGE_TYPE_2D:
                return VK_IMAGE_VIEW_TYPE_2D;
            case VK_IMAGE_TYPE_3D:
                return VK_IMAGE_VIEW_TYPE_3D;
            default:
                VK_ASSERT_FAIL("Invalid Vulkan image type");
                return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
        }
    }


    TextureView& TextureView::SetDebugName(const char* pName)
    {
        Object::SetDebugName(*GetDevice(), (uint64_t)m_view, VK_OBJECT_TYPE_IMAGE_VIEW, pName);
        return *this;
    }


    const char* TextureView::GetDebugName() const
    { 
        return Object::GetDebugName("TextureView");
    }


    Device* TextureView::GetDevice() const
    {
        VK_ASSERT(IsValid());
        return m_pOwner->GetDevice();
    }


    TextureView::TextureView(const TextureViewCreateInfo& info)
        : Object()
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
        std::swap(m_view, view.m_view);
        std::swap(m_type, view.m_type);
        std::swap(m_format, view.m_format);
        std::swap(m_components, view.m_components);
        std::swap(m_subresourceRange, view.m_subresourceRange);

        Object::operator=(std::move(view));

        return *this;
    }


    TextureView& TextureView::Create(const TextureViewCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of texture view %s", GetDebugName());
            Destroy();
        }

        const Texture* pOwner = info.pOwner;

        VK_ASSERT(pOwner && pOwner->IsCreated());

        VkImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = pOwner->Get();
        imageViewCreateInfo.viewType = info.type;
        imageViewCreateInfo.format = info.format;
        imageViewCreateInfo.components = info.components;
        imageViewCreateInfo.subresourceRange = info.subresourceRange;

        m_view = VK_NULL_HANDLE;
        VK_CHECK(vkCreateImageView(pOwner->GetDevice()->Get(), &imageViewCreateInfo, nullptr, &m_view));

        VK_ASSERT_MSG(m_view != VK_NULL_HANDLE, "Failed to create Vulkan texture view");

        SetCreated(true);

        m_pOwner = pOwner;

        m_type = info.type;
        m_format = info.format;
        m_components = info.components;
        m_subresourceRange = info.subresourceRange;

        return *this;
    }


    TextureView& TextureView::Create(const Texture& texture, const VkComponentMapping mapping, const VkImageSubresourceRange& subresourceRange)
    {
        VK_ASSERT(texture.IsCreated());

        TextureViewCreateInfo createInfo = {};
        createInfo.pOwner = &texture;
        createInfo.type = ImageTypeToViewType(texture.GetType());
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

        vkDestroyImageView(GetDevice()->Get(), m_view, nullptr);
        m_view = VK_NULL_HANDLE;

        m_pOwner = nullptr;
        m_type = {};
        m_format = {};
        m_components = {};
        m_subresourceRange = {};

        Object::Destroy();

        return *this;
    }


    bool TextureView::IsValid() const
    {
        return IsCreated() && m_pOwner->IsCreated();
    }


    Texture::Texture(const TextureCreateInfo& info)
        : Object()
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
        
        std::swap(m_image, image.m_image);
        
        std::swap(m_allocation, image.m_allocation);
        std::swap(m_allocInfo, image.m_allocInfo);
        
        std::swap(m_type, image.m_type);
        std::swap(m_extent, image.m_extent);
        std::swap(m_format, image.m_format);
        std::swap(m_mipCount, image.m_mipCount);
        std::swap(m_layersCount, image.m_layersCount);

        std::swap(m_pDevice, image.m_pDevice);

        Object::operator=(std::move(image));

        return *this;
    }


    Texture& Texture::Create(const TextureCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of texture %s", GetDebugName());
            Destroy();
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());
        VK_ASSERT(GetAllocator().IsCreated());
        VK_ASSERT(info.pAllocInfo);

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

        m_image = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
        VK_CHECK(vmaCreateImage(GetAllocator().Get(), &ci, &allocCI, &m_image, &m_allocation, &m_allocInfo));
        
        VK_ASSERT_MSG(m_image != VK_NULL_HANDLE, "Failed to create Vulkan texture");
        VK_ASSERT_MSG(m_allocation != VK_NULL_HANDLE, "Failed to allocate Vulkan texture memory");

        SetCreated(true);

        m_pDevice = info.pDevice;

        m_type = info.type;
        m_extent = info.extent;
        m_format = info.format;
        m_mipCount = info.mipLevels;
        m_layersCount = info.arrayLayers;

        return *this;
    }


    Texture& Texture::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        vmaDestroyImage(GetAllocator().Get(), m_image, m_allocation);
        m_allocation = VK_NULL_HANDLE;
        m_image = VK_NULL_HANDLE;

        m_allocInfo = {};

        m_pDevice = nullptr;

        m_type = {};
        m_extent = {};
        m_format = {};
        m_mipCount = 1;
        m_layersCount = 1;

        Object::Destroy();

        return *this;
    }


    const char* Texture::GetDebugName() const
    { 
        return Object::GetDebugName("Texture");
    }


    Sampler::Sampler(const SamplerCreateInfo& info)
        : Object()
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
        
        std::swap(m_sampler, sampler.m_sampler);
        std::swap(m_pDevice, sampler.m_pDevice);

        Object::operator=(std::move(sampler));

        return *this;
    }


    Sampler& Sampler::Create(const SamplerCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of sampler %s", GetDebugName());
            Destroy();
        }

        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());

        VkDevice vkDevice = info.pDevice->Get();

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

        m_sampler = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSampler(vkDevice, &createInfo, nullptr, &m_sampler));

        VK_ASSERT_MSG(m_sampler != VK_NULL_HANDLE, "Failed to create Vulkan sampler");

        SetCreated(true);

        m_pDevice = info.pDevice;

        return *this;
    }


    Sampler& Sampler::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        VkDevice vkDevice = m_pDevice->Get();

        vkDestroySampler(vkDevice, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;

        m_pDevice = nullptr;

        Object::Destroy();

        return *this;
    }


    const char* Sampler::GetDebugName() const
    {
        return Object::GetDebugName("Sampler");
    }
}
