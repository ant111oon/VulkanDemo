#pragma once

#include "vk_core.h"


namespace vkn
{
    class Surface;
    class PhysicalDevice;
    class Device;


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


    class Swapchain
    {
        friend Swapchain& GetSwapchain();

    public:
        Swapchain(const Swapchain& swapchain) = delete;
        Swapchain(Swapchain&& swapchain) = delete;

        Swapchain& operator=(const Swapchain& swapchain) = delete;
        Swapchain& operator=(Swapchain&& swapchain) = delete;

        bool Create(const SwapchainCreateInfo& info);
        void Destroy();

        bool Recreate(const SwapchainCreateInfo& info);
        bool Resize(uint32_t width, uint32_t height);

        VkSwapchainKHR& Get()
        {
            VK_ASSERT(IsCreated());
            return m_swapchain;
        }

        Device* GetDevice()
        {
            VK_ASSERT(IsCreated());
            return m_pDevice;
        }

        Surface* GetSurface()
        {
            VK_ASSERT(IsCreated());
            return m_pSurface;
        }

        VkImage& GetImage(size_t idx)
        {
            VK_ASSERT(IsCreated());
            VK_ASSERT(idx < m_images.size());
            return m_images[idx];
        }

        VkImageView& GetImageView(size_t idx)
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

        bool IsCreated() const { return m_state.test(FLAG_IS_CREATED); }

    private:
        Swapchain() = default;

        void PullImages();
        void ClearImages();

        void CreateImageViews();
        void DestroyImageViews();

    private:
        enum StateFlags
        {
            FLAG_IS_CREATED,
            FLAG_COUNT,
        };

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

        std::bitset<FLAG_COUNT> m_state = {};
    };


    ENG_FORCE_INLINE Swapchain& GetSwapchain()
    {
        static Swapchain swapchain;
        return swapchain;
    }
}