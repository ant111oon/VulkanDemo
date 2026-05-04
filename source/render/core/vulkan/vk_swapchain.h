#pragma once

#include "vk_surface.h"
#include "vk_texture.h"


namespace vkn
{
    // Swapchain texture wrapper
    class SCTexture : public Handle<VkImage>
    {
        friend class CmdBuffer;
        friend class Swapchain;

    public:
        using Base = Handle<VkImage>;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(SCTexture);
        ENG_DECL_CLASS_NO_MOVABLE(SCTexture);

        SCTexture() = default;
        ~SCTexture();

        Device& GetDevice() const;

        VkImageType GetType() const;
        VkFormat GetFormat() const;

        VkExtent2D GetSize() const;
        uint32_t GetSizeX() const;
        uint32_t GetSizeY() const;

    private:
        SCTexture& Create(Device* pDevice, VkImage image, VkImageType type, VkExtent2D extent, VkFormat format);
        SCTexture& Destroy();

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

        VkImageType m_type = {};
        VkExtent2D m_extent = {};
        VkFormat m_format = {};

        VkImageLayout         m_currLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
        VkPipelineStageFlags2 m_currStageMask = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2        m_currAccessMask = VK_ACCESS_2_NONE;
    };


    // Swapchain texture view wrapper
    class SCTextureView : public Handle<VkImageView>
    {
        friend class Swapchain;

    public:
        using Base = Handle<VkImageView>;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(SCTextureView);
        ENG_DECL_CLASS_NO_MOVABLE(SCTextureView);

        SCTextureView() = default;
        ~SCTextureView();

        Device& GetDevice() const;
        const SCTexture& GetOwner() const;

        VkFormat GetFormat() const;
        VkImageViewType GetType() const;
        VkComponentMapping GetComponentMapping() const;
        VkImageSubresourceRange GetSubresoureRange() const;

        bool IsValid() const;

    private:
        SCTextureView& Create(const SCTexture& texture, const VkComponentMapping mapping, const VkImageSubresourceRange& subresourceRange);
        SCTextureView& Destroy();

    private:
        const SCTexture* m_pOwner = nullptr;

        VkImageViewType         m_type = {};
        VkFormat                m_format = {};
        VkComponentMapping      m_components = {};
        VkImageSubresourceRange m_subresourceRange = {};
    };


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


    class Swapchain : public Handle<VkSwapchainKHR>
    {
        friend Swapchain& GetSwapchain();

    public:
        using Base = Handle<VkSwapchainKHR>;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(Swapchain);
        ENG_DECL_CLASS_NO_MOVABLE(Swapchain);

        ~Swapchain();

        Swapchain& Create(const SwapchainCreateInfo& info, bool& succeded);
        Swapchain& Destroy();

        Swapchain& Recreate(const SwapchainCreateInfo& info, bool& succeded);
        Swapchain& Resize(uint32_t width, uint32_t height, bool& succeded);

        Device& GetDevice() const;
        Surface& GetSurface() const;

        SCTexture& GetTexture(size_t idx);
        SCTextureView& GetTextureView(size_t idx);

        VkFormat GetTextureFormat() const;
        VkColorSpaceKHR GetTextureColorSpace() const;
        VkExtent2D GetTextureExtent() const;

        uint32_t GetTextureCount() const;

    private:
        Swapchain() = default;

        void PullTextures();
        void DestroyTextures();

        void CreateTextureViews();
        void DestroyTextureViews();

    private:
        static constexpr size_t MAX_TEXTURE_COUNT = 4;

    private:
        Device* m_pDevice = nullptr;
        Surface* m_pSurface = nullptr;

        std::array<SCTexture, MAX_TEXTURE_COUNT> m_textures;
        std::array<SCTextureView, MAX_TEXTURE_COUNT> m_textureViews;
        
        VkSwapchainCreateFlagsKHR     m_flags = {};
        uint32_t                      m_minImageCount = {};
        uint32_t                      m_currImageCount = {};
        VkFormat                      m_textureFormat = {};
        VkColorSpaceKHR               m_textureColorSpace = {};
        VkExtent2D                    m_textureExtent = {};
        uint32_t                      m_textureArrayLayers = {};
        VkImageUsageFlags             m_textureUsage = {};
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