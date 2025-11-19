#include "pch.h"

#include "vk_image.h"
#include "vk_utils.h"


namespace vkn
{
    void ImageView::SetDebugName(const char* pName)
    {
        Object::SetDebugName(*GetDevice(), (uint64_t)m_view, VK_OBJECT_TYPE_IMAGE_VIEW, pName);
    }


    const char* ImageView::GetDebugName() const
    { 
        return Object::GetDebugName("ImageView");
    }


    Device* ImageView::GetDevice() const
    {
        VK_ASSERT(IsValid());
        return m_pOwner->GetDevice();
    }


    ImageView::ImageView(const ImageViewCreateInfo& info)
        : Object()
    {
        Create(info);
    }


    ImageView::~ImageView()
    {
        Destroy();
    }


    ImageView::ImageView(ImageView&& view) noexcept
    {
        *this = std::move(view);
    }


    ImageView& ImageView::operator=(ImageView&& view) noexcept
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


    bool ImageView::Create(const ImageViewCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Image view %s is already created", GetDebugName());
            return false;
        }

        const Image* pOwner = info.pOwner;

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

        const bool isCreated = m_view != VK_NULL_HANDLE;
        VK_ASSERT(isCreated);

        SetCreated(isCreated);

        m_pOwner = pOwner;

        m_type = info.type;
        m_format = info.format;
        m_components = info.components;
        m_subresourceRange = info.subresourceRange;

        return isCreated;
    }


    void ImageView::Destroy()
    {
        if (!IsCreated()) {
            return;
        }

        vkDestroyImageView(GetDevice()->Get(), m_view, nullptr);
        m_view = VK_NULL_HANDLE;

        m_pOwner = nullptr;
        m_type = {};
        m_format = {};
        m_components = {};
        m_subresourceRange = {};

        Object::Destroy();
    }


    bool ImageView::IsValid() const
    {
        return IsCreated() && m_pOwner->IsCreated();
    }


    Image::Image(const ImageCreateInfo& info)
        : Object()
    {
        Create(info);
    }


    Image::Image(Image&& image) noexcept
    {
        *this = std::move(image);
    }


    Image::~Image()
    {
        Destroy();
    }


    Image& Image::operator=(Image&& image) noexcept
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

        std::swap(m_pDevice, image.m_pDevice);

        Object::operator=(std::move(image));

        return *this;
    }


    bool Image::Create(const ImageCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Image %s is already created", GetDebugName());
            return false;
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
        
        const bool isCreated = m_image != VK_NULL_HANDLE && m_allocation != VK_NULL_HANDLE;
        VK_ASSERT(isCreated);

        SetCreated(isCreated);

        m_pDevice = info.pDevice;

        m_type = info.type;
        m_extent = info.extent;
        m_format = info.format;

        return isCreated;
    }


    void Image::Destroy()
    {
        if (!IsCreated()) {
            return;
        }

        vmaDestroyImage(GetAllocator().Get(), m_image, m_allocation);
        m_allocation = VK_NULL_HANDLE;
        m_image = VK_NULL_HANDLE;

        m_allocInfo = {};

        m_pDevice = nullptr;

        m_type = {};
        m_extent = {};
        m_format = {};

        Object::Destroy();
    }


    const char* Image::GetDebugName() const
    { 
        return Object::GetDebugName("Image");
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


    bool Sampler::Create(const SamplerCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Image %s is already created", GetDebugName());
            return false;
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

        const bool isCreated = m_sampler != VK_NULL_HANDLE;
        VK_ASSERT(m_sampler);

        SetCreated(isCreated);

        m_pDevice = info.pDevice;

        return isCreated;
    }


    void Sampler::Destroy()
    {
        if (!IsCreated()) {
            return;
        }

        VkDevice vkDevice = m_pDevice->Get();

        vkDestroySampler(vkDevice, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;

        m_pDevice = nullptr;

        Object::Destroy();
    }


    const char* Sampler::GetDebugName() const
    {
        return Object::GetDebugName("Sampler");
    }
}
