#pragma once

#include "vk_object.h"
#include "vk_device.h"


namespace vkn
{
    class Image;


    struct ImageViewCreateInfo
    {
        const Image*            pOwner;

        VkImageViewType         type;
        VkFormat                format;
        VkComponentMapping      components;
        VkImageSubresourceRange subresourceRange;
    };


    class ImageView : public Object
    {
    public:
        ENG_DECL_CLASS_NO_COPIABLE(ImageView);

        ImageView() = default;
        ImageView(const ImageViewCreateInfo& info);

        ImageView(ImageView&& view) noexcept;
        ImageView& operator=(ImageView&& view) noexcept;

        bool Create(const ImageViewCreateInfo& info);
        void Destroy();

        void SetDebugName(const char* pName);
        const char* GetDebugName() const;

        const Image* GetOwner() const
        {
            VK_ASSERT(IsCreated());
            return m_pOwner;
        }

        Device* GetDevice() const;

        VkImageView Get() const
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
        const Image* m_pOwner = nullptr;

        VkImageView m_view = VK_NULL_HANDLE;

        VkImageViewType         m_type = {};
        VkFormat                m_format = {};
        VkComponentMapping      m_components = {};
        VkImageSubresourceRange m_subresourceRange = {};
    };


    struct ImageMemoryAllocateInfo
    {
        VkMemoryAllocateFlags flags;
        VkMemoryPropertyFlags properties;
    };


    struct ImageCreateInfo
    {
        Device* pDevice;

        VkImageType           type;
        VkExtent3D            extent;
        VkFormat              format;
        VkImageUsageFlags     usage; 
        VkImageLayout         initialLayout;
        VkImageCreateFlags    flags;
        uint32_t              mipLevels;
        uint32_t              arrayLayers;
        VkSampleCountFlagBits samples;
        VkImageTiling         tiling;
        
        ImageMemoryAllocateInfo memAllocInfo;
    };


    class Image : public Object
    {
    public:
        ENG_DECL_CLASS_NO_COPIABLE(Image);

        Image() = default;
        Image(const ImageCreateInfo& info);

        Image(Image&& image) noexcept;
        Image& operator=(Image&& image) noexcept;

        bool Create(const ImageCreateInfo& info);
        void Destroy();

        void SetDebugName(const char* pName);
        const char* GetDebugName() const;

        Device* GetDevice() const
        {
            VK_ASSERT(IsCreated());
            return m_pDevice;
        }

        VkImage Get() const
        {
            VK_ASSERT(IsCreated());
            return m_image;
        }

        VkDeviceMemory GetMemory() const
        {
            VK_ASSERT(IsCreated());
            return m_memory;
        }

        VkImageType GetType() const
        {
            VK_ASSERT(IsCreated());
            return m_type;
        }

        const VkExtent3D& GetExtent() const
        {
            VK_ASSERT(IsCreated());
            return m_extent;    
        }

        VkFormat GetFormat() const
        {
            VK_ASSERT(IsCreated());
            return m_format;    
        }

    private:
        Device* m_pDevice = nullptr;

        VkImage        m_image = VK_NULL_HANDLE;
        VkDeviceMemory m_memory = VK_NULL_HANDLE;

        VkImageType m_type = {};
        VkExtent3D m_extent = {};
        VkFormat m_format = {};
    };
}