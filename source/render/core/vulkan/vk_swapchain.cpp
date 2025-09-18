#include "pch.h"

#include "vk_swapchain.h"


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


    bool Swapchain::Create(const SwapchainCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Use Swapchain::Recreate if you want to recreate swapchain");
            return true;
        }

        return Recreate(info);
    }


    void Swapchain::Destroy()
    {
        if (!IsCreated()) {
            return;
        }

        DestroyImageViews();
        ClearImages();

        vkDestroySwapchainKHR(m_pDevice->Get(), m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;

        m_pDevice = nullptr;
        m_pSurface = nullptr;
        
        m_flags = {};
        m_minImageCount = {};
        m_imageFormat = {};
        m_imageColorSpace = {};
        m_imageExtent = {};
        m_imageArrayLayers = {};
        m_imageUsage = {};
        m_transform = {};
        m_compositeAlpha = {};
        m_presentMode = {};

        Object::Destroy();
    }


    bool Swapchain::Recreate(const SwapchainCreateInfo& info)
    {
        VkSwapchainCreateInfoKHR swapchainCreateInfo = CreateSwapchainCreateInfo(info, *this);

        const VkExtent2D newExtent = swapchainCreateInfo.imageExtent;

        if (newExtent.width == 0 || newExtent.height == 0) {
            return true;
        }

        if (newExtent.width == m_imageExtent.width && newExtent.height == m_imageExtent.height) {
            return true;
        }

        VK_CHECK(vkDeviceWaitIdle(info.pDevice->Get()));

        VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSwapchainKHR(info.pDevice->Get(), &swapchainCreateInfo, nullptr, &newSwapchain));

        const bool isCreated = newSwapchain != VK_NULL_HANDLE;
        VK_ASSERT(isCreated);

        if (m_swapchain) {
            vkDestroySwapchainKHR(info.pDevice->Get(), m_swapchain, nullptr);
        }

        SetCreated(isCreated);

        if (!isCreated) {
            return false;
        }

        m_pSurface = info.pSurface;
        m_pDevice = info.pDevice;

        m_swapchain = newSwapchain;

        m_flags = swapchainCreateInfo.flags;
        m_minImageCount = swapchainCreateInfo.minImageCount;
        m_imageFormat = swapchainCreateInfo.imageFormat;
        m_imageColorSpace = swapchainCreateInfo.imageColorSpace;
        m_imageExtent = swapchainCreateInfo.imageExtent;
        m_imageArrayLayers = swapchainCreateInfo.imageArrayLayers;
        m_imageUsage = swapchainCreateInfo.imageUsage;
        m_transform = swapchainCreateInfo.preTransform;
        m_compositeAlpha = swapchainCreateInfo.compositeAlpha;
        m_presentMode = swapchainCreateInfo.presentMode;

        DestroyImageViews();
        PullImages();
        CreateImageViews();

        return isCreated;
    }


    bool Swapchain::Resize(uint32_t width, uint32_t height)
    {
        if (!IsCreated()) {
            VK_ASSERT_FAIL("Swapchain is not created. Can't resize swapchain.");
            return false;
        }

        SwapchainCreateInfo createInfo = {};
        createInfo.pDevice = m_pDevice;
        createInfo.pSurface = m_pSurface;

        createInfo.width = width;
        createInfo.height = height;

        createInfo.flags = m_flags;
        createInfo.minImageCount = m_minImageCount;
        createInfo.imageFormat = m_imageFormat;
        createInfo.imageColorSpace = m_imageColorSpace;
        createInfo.imageArrayLayers = m_imageArrayLayers;
        createInfo.imageUsage = m_imageUsage;
        createInfo.transform = m_transform;
        createInfo.compositeAlpha = m_compositeAlpha;
        createInfo.presentMode = m_presentMode;

        return Recreate(createInfo);
    }


    void Swapchain::PullImages()
    {
        VkDevice vkDevice = m_pDevice->Get();

        VK_ASSERT(vkDevice != VK_NULL_HANDLE);
        VK_ASSERT(m_swapchain != VK_NULL_HANDLE);

        uint32_t swapchainImagesCount = 0;
        VK_CHECK(vkGetSwapchainImagesKHR(vkDevice, m_swapchain, &swapchainImagesCount, nullptr));
        m_images.resize(swapchainImagesCount);
        VK_CHECK(vkGetSwapchainImagesKHR(vkDevice, m_swapchain, &swapchainImagesCount, m_images.data()));
    }


    void Swapchain::ClearImages()
    {
        m_images.clear();
    }


    void Swapchain::CreateImageViews()
    {
        VK_ASSERT(!m_images.empty());

        VkDevice vkDevice = m_pDevice->Get();
        VK_ASSERT(vkDevice != VK_NULL_HANDLE);
        
        m_imageViews.resize(m_images.size());

        for (size_t i = 0; i < m_images.size(); ++i) {
            VkImageViewCreateInfo imageViewCreateInfo = {};
            imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            imageViewCreateInfo.image = m_images[i];
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            imageViewCreateInfo.format = m_imageFormat;
            imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
            imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
            imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
            imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
            imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
            imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
            imageViewCreateInfo.subresourceRange.layerCount = 1;
            imageViewCreateInfo.subresourceRange.levelCount = 1;

            VkImageView& view = m_imageViews[i];

            VK_CHECK(vkCreateImageView(vkDevice, &imageViewCreateInfo, nullptr, &view));
        }
    }


    void Swapchain::DestroyImageViews()
    {
        VkDevice vkDevice = m_pDevice->Get();

        for (VkImageView& view : m_imageViews) {
            vkDestroyImageView(vkDevice, view, nullptr);
            view = VK_NULL_HANDLE;
        }

        m_imageViews.clear();
    }
}