#pragma once

#include "vk_device.h"
#include "vk_surface.h"

#include "vk_texture.h"


namespace vkn
{
    // Swapchain texture wrapper
    class SCTexture : public Object
    {
        friend class CmdBuffer;
        friend class Swapchain;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(SCTexture);
        ENG_DECL_CLASS_NO_MOVABLE(SCTexture);

        SCTexture() = default;
        ~SCTexture();

        const char* GetDebugName() const
        {
            return Object::GetDebugName("SCTexture");
        }

        Device* GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_pDevice;
        }

        const VkImage& Get() const
        {
            VK_ASSERT(IsCreated());
            return m_image;
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

        VkExtent2D GetSize() const
        {
            VK_ASSERT(IsCreated());
            return m_extent;    
        }

        uint32_t GetSizeX() const { return GetSize().width; }
        uint32_t GetSizeY() const { return GetSize().height; }

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

        template <typename... Args>
        SCTexture& SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(*GetDevice(), (uint64_t)m_image, VK_OBJECT_TYPE_IMAGE, pFmt, std::forward<Args>(args)...);
            return *this;
        }

    private:
        Device* m_pDevice = nullptr;

        VkImage m_image = VK_NULL_HANDLE;

        VkImageType m_type = {};
        VkExtent2D m_extent = {};
        VkFormat m_format = {};

        VkImageLayout         m_currLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
        VkPipelineStageFlags2 m_currStageMask = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2        m_currAccessMask = VK_ACCESS_2_NONE;
    };


    // Swapchain texture view wrapper
    class SCTextureView : public Object
    {
        friend class Swapchain;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(SCTextureView);
        ENG_DECL_CLASS_NO_MOVABLE(SCTextureView);

        SCTextureView() = default;
        ~SCTextureView();

        const char* GetDebugName() const { return Object::GetDebugName("SCTextureView"); }

        const SCTexture* GetOwner() const
        {
            VK_ASSERT(IsCreated());
            return m_pOwner;
        }

        Device* GetDevice() const;

        const VkImageView& Get() const
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
        SCTextureView& Create(const SCTexture& texture, const VkComponentMapping mapping, const VkImageSubresourceRange& subresourceRange);
        SCTextureView& Destroy();

        template <typename... Args>
        SCTextureView& SetDebugName(const char* pFmt, Args&&... args)
        {
            Object::SetDebugName(*GetDevice(), (uint64_t)m_view, VK_OBJECT_TYPE_IMAGE_VIEW, pFmt, std::forward<Args>(args)...);
            return *this;
        }

    private:
        const SCTexture* m_pOwner = nullptr;

        VkImageView m_view = VK_NULL_HANDLE;

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

        const VkSwapchainKHR& Get() const
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

        SCTexture& GetTexture(size_t idx)
        {
            VK_ASSERT(IsCreated());
            VK_ASSERT(idx < GetTextureCount());
            return m_textures[idx];
        }

        SCTextureView& GetTextureView(size_t idx)
        {
            VK_ASSERT(IsCreated());
            VK_ASSERT(idx < GetTextureCount());
            return m_textureViews[idx];
        }

        VkFormat GetTextureFormat() const
        {
            VK_ASSERT(IsCreated());
            return m_textureFormat;
        }

        VkColorSpaceKHR GetTextureColorSpace() const
        {
            VK_ASSERT(IsCreated());
            return m_textureColorSpace;
        }

        VkExtent2D GetTextureExtent() const
        {
            VK_ASSERT(IsCreated());
            return m_textureExtent;
        }

        uint32_t GetTextureCount() const
        {
            VK_ASSERT(IsCreated());
            return m_currImageCount;
        }

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

        VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;

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