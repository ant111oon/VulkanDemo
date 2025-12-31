#pragma once

#include "vk_object.h"
#include "vk_surface.h"
#include "vk_phys_device.h"
#include "vk_device.h"


namespace vkn
{
    struct SwapchainCreateInfo
    {
        Device*  pDevice;
        Surface* pSurface;

        uint32_t width;
        uint32_t height;

        VkSwapchainCreateFlagsKHR     flags;
        uint32_t                      minImageCount;
        VkFormat                      imageFormat;
        VkColorSpaceKHR               imageColorSpace;
        uint32_t                      imageArrayLayers;
        VkImageUsageFlags             imageUsage;
        VkSurfaceTransformFlagBitsKHR transform;
        VkCompositeAlphaFlagBitsKHR   compositeAlpha;
        VkPresentModeKHR              presentMode;
    };


    class Swapchain : public Object
    {
        friend Swapchain& GetSwapchain();

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Swapchain);
        ENG_DECL_CLASS_NO_MOVABLE(Swapchain);

        ~Swapchain();

        Swapchain& Create(const SwapchainCreateInfo& info, bool& succeded);
        Swapchain& Destroy();

        Swapchain& Recreate(const SwapchainCreateInfo& info, bool& succeded);
        Swapchain& Resize(uint32_t width, uint32_t height, bool& succeded);

        VkSwapchainKHR Get() const
        {
            VK_ASSERT(IsCreated());
            return m_swapchain;
        }

        Device* GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_pDevice;
        }

        Surface* GetSurface() const
        {
            VK_ASSERT(IsCreated());
            return m_pSurface;
        }

        VkImage GetImage(size_t idx) const
        {
            VK_ASSERT(IsCreated());
            VK_ASSERT(idx < m_images.size());
            return m_images[idx];
        }

        VkImageView GetImageView(size_t idx) const
        {
            VK_ASSERT(IsCreated());
            VK_ASSERT(idx < m_imageViews.size());
            return m_imageViews[idx];
        }

        VkFormat GetImageFormat() const
        {
            VK_ASSERT(IsCreated());
            return m_imageFormat;
        }

        VkColorSpaceKHR GetImageColorSpace() const
        {
            VK_ASSERT(IsCreated());
            return m_imageColorSpace;
        }

        VkExtent2D GetImageExtent() const
        {
            VK_ASSERT(IsCreated());
            return m_imageExtent;
        }

        size_t GetImageCount() const
        {
            VK_ASSERT(IsCreated());
            return m_images.size();
        }

    private:
        Swapchain() = default;

        void PullImages();
        void ClearImages();

        void CreateImageViews();
        void DestroyImageViews();

    private:
        Device* m_pDevice = nullptr;
        Surface* m_pSurface = nullptr;

        VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;

        std::vector<VkImage> m_images;
        std::vector<VkImageView> m_imageViews;
        
        VkSwapchainCreateFlagsKHR     m_flags = {};
        uint32_t                      m_minImageCount = {};
        VkFormat                      m_imageFormat = {};
        VkColorSpaceKHR               m_imageColorSpace = {};
        VkExtent2D                    m_imageExtent = {};
        uint32_t                      m_imageArrayLayers = {};
        VkImageUsageFlags             m_imageUsage = {};
        VkSurfaceTransformFlagBitsKHR m_transform = {};
        VkCompositeAlphaFlagBitsKHR   m_compositeAlpha = {};
        VkPresentModeKHR              m_presentMode = {};
    };


    ENG_FORCE_INLINE Swapchain& GetSwapchain()
    {
        static Swapchain swapchain;
        return swapchain;
    }
}