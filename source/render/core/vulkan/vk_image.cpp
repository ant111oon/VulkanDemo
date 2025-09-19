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


    Image& Image::operator=(Image&& image) noexcept
    {
        if (this == &image) {
            return *this;
        }

        if (IsCreated()) {
            Destroy();
        }
        
        std::swap(m_image, image.m_image);
        std::swap(m_memory, image.m_memory);
        
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

        VkDevice vkDevice = info.pDevice->Get();

        VkImageCreateInfo imageCreateInfo = {};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.flags = info.flags;
        imageCreateInfo.imageType = info.type;
        imageCreateInfo.format = info.format;
        imageCreateInfo.extent = info.extent;
        imageCreateInfo.mipLevels = info.mipLevels;
        imageCreateInfo.arrayLayers = info.arrayLayers;
        imageCreateInfo.samples = info.samples;
        imageCreateInfo.tiling = info.tiling;
        imageCreateInfo.usage = info.usage;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.queueFamilyIndexCount = 0;
        imageCreateInfo.pQueueFamilyIndices = nullptr;
        imageCreateInfo.initialLayout = info.initialLayout;

        m_image = VK_NULL_HANDLE;
        VK_CHECK(vkCreateImage(vkDevice, &imageCreateInfo, nullptr, &m_image));
        
        const bool isImageCreated = m_image != VK_NULL_HANDLE;
        VK_ASSERT(isImageCreated);

        VkImageMemoryRequirementsInfo2 memRequirementsInfo = {};
        memRequirementsInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
        memRequirementsInfo.image = m_image;

        VkMemoryRequirements2 memRequirements = {};
        memRequirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
        vkGetImageMemoryRequirements2(vkDevice, &memRequirementsInfo, &memRequirements);

        VkMemoryAllocateFlagsInfo memAllocFlagsInfo = {};
        memAllocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        memAllocFlagsInfo.flags = info.memAllocInfo.flags;

        VkMemoryAllocateInfo memAllocInfo = {};
        memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memAllocInfo.pNext = &memAllocFlagsInfo;
        memAllocInfo.allocationSize = memRequirements.memoryRequirements.size;
        memAllocInfo.memoryTypeIndex = utils::FindMemoryType(*info.pDevice->GetPhysDevice(),
            memRequirements.memoryRequirements.memoryTypeBits, info.memAllocInfo.properties);
        
        VK_ASSERT_MSG(memAllocInfo.memoryTypeIndex != UINT32_MAX, "Failed to find required memory type index");

        m_memory = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateMemory(vkDevice, &memAllocInfo, nullptr, &m_memory));

        const bool isMemoryAllocated = m_memory != VK_NULL_HANDLE;
        VK_ASSERT(isMemoryAllocated);

        VkBindImageMemoryInfo bindInfo = {};
        bindInfo.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
        bindInfo.image = m_image;
        bindInfo.memory = m_memory;

        VK_CHECK(vkBindImageMemory2(vkDevice, 1, &bindInfo));

        const bool isCreated = isImageCreated && isMemoryAllocated;
        VK_ASSERT(isCreated);

        SetCreated(isMemoryAllocated);

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

        VkDevice vkDevice = m_pDevice->Get();

        vkFreeMemory(vkDevice, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;

        vkDestroyImage(vkDevice, m_image, nullptr);
        m_image = VK_NULL_HANDLE;

        m_pDevice = nullptr;

        m_type = {};
        m_extent = {};
        m_format = {};

        Object::Destroy();
    }


    void Image::SetDebugName(const char* pName)
    {
        Object::SetDebugName(*m_pDevice, (uint64_t)m_image, VK_OBJECT_TYPE_IMAGE, pName);
    }


    const char* Image::GetDebugName() const
    { 
        return Object::GetDebugName("Image");
    }
}
