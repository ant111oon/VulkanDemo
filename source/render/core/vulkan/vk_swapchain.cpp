#include "pch.h"

#include "vk_swapchain.h"
#include "vk_utils.h"


namespace vkn
{
    static bool CheckSurfaceFormatSupport(VkPhysicalDevice vkPhysDevice, VkSurfaceKHR vkSurface, VkSurfaceFormatKHR format)
    {
        uint32_t surfaceFormatsCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysDevice, vkSurface, &surfaceFormatsCount, nullptr));
        std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatsCount);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysDevice, vkSurface, &surfaceFormatsCount, surfaceFormats.data()));

        if (surfaceFormatsCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
            return true;
        }

        for (VkSurfaceFormatKHR fmt : surfaceFormats) {
            if (fmt.format == format.format && fmt.colorSpace == format.colorSpace) {
                return true;
            }
        }

        return false;
    }


    static bool CheckPresentModeSupport(VkPhysicalDevice vkPhysDevice, VkSurfaceKHR vkSurface, VkPresentModeKHR presentMode)
    {
        uint32_t presentModesCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysDevice, vkSurface, &presentModesCount, nullptr));
        std::vector<VkPresentModeKHR> presentModes(presentModesCount);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysDevice, vkSurface, &presentModesCount, presentModes.data()));

        for (VkPresentModeKHR mode : presentModes) {
            if (mode == presentMode) {
                return true;
            }
        }

        return false;
    }


    static VkExtent2D EvaluateExtent(uint32_t requiredWidth, uint32_t requiredHeight, const VkSurfaceCapabilitiesKHR& surfCapabilities)
    {
        VkExtent2D extent = {};

        if (surfCapabilities.currentExtent.width != UINT32_MAX && surfCapabilities.currentExtent.height != UINT32_MAX) {
            extent = surfCapabilities.currentExtent;
        } else {
            extent.width = std::clamp(requiredWidth, surfCapabilities.minImageExtent.width, surfCapabilities.maxImageExtent.width);
            extent.height = std::clamp(requiredHeight, surfCapabilities.minImageExtent.height, surfCapabilities.maxImageExtent.height);
        }

        return extent;
    }
    
    
    VkSwapchainCreateInfoKHR CreateSwapchainCreateInfo(const SwapchainCreateInfo& info, Swapchain& oldSwapchain)
    {
        VK_ASSERT(info.pSurface && info.pSurface->IsCreated());
        VK_ASSERT(info.pDevice && info.pDevice->IsCreated());

        VkSurfaceKHR vkSurface = info.pSurface->Get();
        VkPhysicalDevice vkPhysDevice = info.pDevice->GetPhysDevice()->Get();

        VkSurfaceFormatKHR surfaceFormat = {};
        surfaceFormat.format = info.imageFormat;
        surfaceFormat.colorSpace = info.imageColorSpace;

        VK_ASSERT_MSG(CheckSurfaceFormatSupport(vkPhysDevice, vkSurface, surfaceFormat),
            "Unsupported swapchain surface format: %s, color space: %s",
            string_VkFormat(surfaceFormat.format),
            string_VkColorSpaceKHR(surfaceFormat.colorSpace));

        VkSurfaceCapabilitiesKHR surfCapabilities = {};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysDevice, vkSurface, &surfCapabilities));

        uint32_t minImageCount = std::max(info.minImageCount, surfCapabilities.minImageCount + 1);
        if (surfCapabilities.maxImageCount != 0) {
            minImageCount = std::min(minImageCount, surfCapabilities.maxImageCount);
        }

        const VkSurfaceTransformFlagBitsKHR trs = (surfCapabilities.supportedTransforms & info.transform) ? info.transform : surfCapabilities.currentTransform;

        VK_ASSERT(minImageCount >= surfCapabilities.minImageCount);
        if (surfCapabilities.maxImageCount != 0) {
            VK_ASSERT(minImageCount <= surfCapabilities.maxImageCount);
        }
        VK_ASSERT((trs & surfCapabilities.supportedTransforms) == trs);
        VK_ASSERT((info.imageUsage & surfCapabilities.supportedUsageFlags) == info.imageUsage);
        VK_ASSERT((info.compositeAlpha & surfCapabilities.supportedCompositeAlpha) == info.compositeAlpha);

        const VkPresentModeKHR presentMode = CheckPresentModeSupport(vkPhysDevice, vkSurface, info.presentMode) ? info.presentMode : VK_PRESENT_MODE_FIFO_KHR;

        VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
        swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreateInfo.flags = info.flags;
        swapchainCreateInfo.oldSwapchain = oldSwapchain.IsCreated() ? oldSwapchain.Get() : VK_NULL_HANDLE;
        swapchainCreateInfo.surface = vkSurface;
        swapchainCreateInfo.imageArrayLayers = info.imageArrayLayers;
        swapchainCreateInfo.compositeAlpha = info.compositeAlpha;
        swapchainCreateInfo.imageUsage = info.imageUsage;
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Since we have one queue for graphics, compute and transfer
        swapchainCreateInfo.imageExtent = EvaluateExtent(info.width, info.height, surfCapabilities);
        swapchainCreateInfo.imageFormat = surfaceFormat.format;
        swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
        swapchainCreateInfo.minImageCount = minImageCount;
        swapchainCreateInfo.preTransform = trs;
        swapchainCreateInfo.presentMode = presentMode;
        
        swapchainCreateInfo.clipped = VK_TRUE;

        return swapchainCreateInfo;
    }


    SCTexture::~SCTexture()
    {
        Destroy();
    }


    SCTexture& SCTexture::Create(Device* pDevice, VkImage image, VkImageType type, VkExtent2D extent, VkFormat format)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of swapchain texture %s, Old Size: [%u, %u]; New Size: [%u, %u]", 
                GetDebugName(), m_extent.width, m_extent.height, extent.width, extent.height);
            
            Destroy();
        }

        VK_ASSERT(pDevice && pDevice->IsCreated());
        VK_ASSERT(image != VK_NULL_HANDLE);

        m_pDevice = pDevice;
        m_image = image;        
        m_type = type;
        m_extent = extent;
        m_format = format;

        m_currLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_currStageMask = VK_PIPELINE_STAGE_2_NONE;
        m_currAccessMask = VK_ACCESS_2_NONE;

        SetCreated(true);

        return *this;
    }


    SCTexture& SCTexture::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        m_pDevice = nullptr;
        m_image = VK_NULL_HANDLE;
        m_type = {};
        m_extent = {};
        m_format = {};

        m_currLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_currStageMask = VK_PIPELINE_STAGE_2_NONE;
        m_currAccessMask = VK_ACCESS_2_NONE;

        Object::Destroy();

        return *this;        
    }


    void SCTexture::Transit(VkImageLayout dstLayout, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask)
    {
        VK_ASSERT(IsCreated());
        
        m_currLayout = dstLayout;
        m_currStageMask = dstStageMask;
        m_currAccessMask = dstAccessMask;
    }


    SCTextureView::~SCTextureView()
    {
        Destroy();
    }


    Device* SCTextureView::GetDevice() const
    {
        VK_ASSERT(IsValid());
        return m_pOwner->GetDevice();
    }


    bool SCTextureView::IsValid() const
    {
        return IsCreated() && m_pOwner->IsCreated();
    }


    SCTextureView& SCTextureView::Create(const SCTexture& texture, const VkComponentMapping mapping, const VkImageSubresourceRange& subresourceRange)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of swapchain texture view %s", GetDebugName());
            Destroy();
        }

        const SCTexture* pOwner = &texture;

        VK_ASSERT(pOwner && pOwner->IsCreated());

        VkImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = pOwner->Get();
        imageViewCreateInfo.viewType = utils::ImageTypeToViewType(pOwner->GetType());
        imageViewCreateInfo.format = pOwner->GetFormat();
        imageViewCreateInfo.components = mapping;
        imageViewCreateInfo.subresourceRange = subresourceRange;

        m_view = VK_NULL_HANDLE;
        VK_CHECK(vkCreateImageView(pOwner->GetDevice()->Get(), &imageViewCreateInfo, nullptr, &m_view));

        VK_ASSERT_MSG(m_view != VK_NULL_HANDLE, "Failed to create Vulkan swapchain texture view");

        SetCreated(true);

        m_pOwner = pOwner;

        m_type = imageViewCreateInfo.viewType;
        m_format = imageViewCreateInfo.format;
        m_components = imageViewCreateInfo.components;
        m_subresourceRange = imageViewCreateInfo.subresourceRange;

        return *this;
    }


    SCTextureView& SCTextureView::Destroy()
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


    Swapchain::~Swapchain()
    {
        Destroy();
    }


    Swapchain& Swapchain::Create(const SwapchainCreateInfo& info, bool& succeded)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Use Swapchain::Recreate if you want to recreate swapchain");
            succeded = false;
            return *this;
        }

        return Recreate(info, succeded);
    }


    Swapchain& Swapchain::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        DestroyTextureViews();
        DestroyTextures();

        vkDestroySwapchainKHR(m_pDevice->Get(), m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;

        m_pDevice = nullptr;
        m_pSurface = nullptr;
        
        m_flags = {};
        m_minImageCount = {};
        m_currImageCount = {};
        m_textureFormat = {};
        m_textureColorSpace = {};
        m_textureExtent = {};
        m_textureArrayLayers = {};
        m_textureUsage = {};
        m_transform = {};
        m_compositeAlpha = {};
        m_presentMode = {};

        Object::Destroy();

        return *this;
    }


    Swapchain& Swapchain::Recreate(const SwapchainCreateInfo& info, bool& succeded)
    {
        VkSwapchainCreateInfoKHR swapchainCreateInfo = CreateSwapchainCreateInfo(info, *this);

        const VkExtent2D newExtent = swapchainCreateInfo.imageExtent;

        if (newExtent.width == 0 || newExtent.height == 0) {
            succeded = false;
            return *this;
        }

        if (newExtent.width == m_textureExtent.width && newExtent.height == m_textureExtent.height) {
            succeded = true;
            return *this;
        }

        VK_CHECK(vkDeviceWaitIdle(info.pDevice->Get()));

        VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSwapchainKHR(info.pDevice->Get(), &swapchainCreateInfo, nullptr, &newSwapchain));

        if (newSwapchain == VK_NULL_HANDLE) {
            VK_ASSERT_FAIL("New swapchain is VK_NULL_HANDLE");
            succeded = false;
            return *this;
        }

        if (m_swapchain) {
            vkDestroySwapchainKHR(info.pDevice->Get(), m_swapchain, nullptr);
        }

        SetCreated(true);
        succeded = true;

        m_pSurface = info.pSurface;
        m_pDevice = info.pDevice;

        m_swapchain = newSwapchain;

        m_flags = swapchainCreateInfo.flags;
        m_minImageCount = swapchainCreateInfo.minImageCount;
        m_textureFormat = swapchainCreateInfo.imageFormat;
        m_textureColorSpace = swapchainCreateInfo.imageColorSpace;
        m_textureExtent = swapchainCreateInfo.imageExtent;
        m_textureArrayLayers = swapchainCreateInfo.imageArrayLayers;
        m_textureUsage = swapchainCreateInfo.imageUsage;
        m_transform = swapchainCreateInfo.preTransform;
        m_compositeAlpha = swapchainCreateInfo.compositeAlpha;
        m_presentMode = swapchainCreateInfo.presentMode;

        DestroyTextureViews();
        PullTextures();
        CreateTextureViews();

        return *this;
    }


    Swapchain& Swapchain::Resize(uint32_t width, uint32_t height, bool& succeded)
    {
        if (!IsCreated()) {
            VK_ASSERT_FAIL("Swapchain is not created. Can't resize swapchain.");
            succeded = false;
            return *this;
        }

        SwapchainCreateInfo createInfo = {};
        createInfo.pDevice = m_pDevice;
        createInfo.pSurface = m_pSurface;

        createInfo.width = width;
        createInfo.height = height;

        createInfo.flags = m_flags;
        createInfo.minImageCount = m_minImageCount;
        createInfo.imageFormat = m_textureFormat;
        createInfo.imageColorSpace = m_textureColorSpace;
        createInfo.imageArrayLayers = m_textureArrayLayers;
        createInfo.imageUsage = m_textureUsage;
        createInfo.transform = m_transform;
        createInfo.compositeAlpha = m_compositeAlpha;
        createInfo.presentMode = m_presentMode;

        return Recreate(createInfo, succeded);
    }


    void Swapchain::PullTextures()
    {
        VkDevice vkDevice = m_pDevice->Get();

        VK_ASSERT(vkDevice != VK_NULL_HANDLE);
        VK_ASSERT(m_swapchain != VK_NULL_HANDLE);

        VK_CHECK(vkGetSwapchainImagesKHR(vkDevice, m_swapchain, &m_currImageCount, nullptr));
        
        std::vector<VkImage> images(m_currImageCount);
        VK_CHECK(vkGetSwapchainImagesKHR(vkDevice, m_swapchain, &m_currImageCount, images.data()));

        for (uint32_t i = 0; i < m_currImageCount; ++i) {
            m_textures[i].Create(m_pDevice, images[i], VK_IMAGE_TYPE_2D, m_textureExtent, m_textureFormat)
                .SetDebugName("SWAPCHAIN_TEXTURE_%u", i);
        }
    }


    void Swapchain::DestroyTextures()
    {
        for (uint32_t i = 0; i < m_currImageCount; ++i) {
            m_textures[i].Destroy();
        }
    }


    void Swapchain::CreateTextureViews()
    {
        VK_ASSERT(!m_textures.empty());

        VkDevice vkDevice = m_pDevice->Get();
        VK_ASSERT(vkDevice != VK_NULL_HANDLE);

        for (size_t i = 0; i < m_currImageCount; ++i) {
            const VkComponentMapping components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

            VkImageSubresourceRange subresourceRange = {};
            subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            subresourceRange.baseArrayLayer = 0;
            subresourceRange.baseMipLevel = 0;
            subresourceRange.layerCount = 1;
            subresourceRange.levelCount = 1;

            m_textureViews[i].Create(m_textures[i], components, subresourceRange).SetDebugName("SWAPCHAIN_TEXTURE_VIEW_%u", i);
        }
    }


    void Swapchain::DestroyTextureViews()
    {
        for (uint32_t i = 0; i < m_currImageCount; ++i) {
            m_textureViews[i].Destroy();
        }
    }
}