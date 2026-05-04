#pragma once

#include "vk_core.h"


namespace vkn
{
    class Device;
}


namespace vkn::utils
{
    template <typename VkHandle>
    inline VkObjectType GetObjectType() { static_assert(false, "Unknown Vulkan Handle"); return VK_OBJECT_TYPE_UNKNOWN; }

    template <>
    inline VkObjectType GetObjectType<VkInstance>() { return VK_OBJECT_TYPE_INSTANCE; }

    template <>
    inline VkObjectType GetObjectType<VkPhysicalDevice>() { return VK_OBJECT_TYPE_PHYSICAL_DEVICE; }

    template <>
    inline VkObjectType GetObjectType<VkDevice>() { return VK_OBJECT_TYPE_DEVICE; }

    template <>
    inline VkObjectType GetObjectType<VkQueue>() { return VK_OBJECT_TYPE_QUEUE; }

    template <>
    inline VkObjectType GetObjectType<VkCommandBuffer>() { return VK_OBJECT_TYPE_COMMAND_BUFFER; }

    template <>
    inline VkObjectType GetObjectType<VkFence>() { return VK_OBJECT_TYPE_FENCE; }

    template <>
    inline VkObjectType GetObjectType<VkSemaphore>() { return VK_OBJECT_TYPE_SEMAPHORE; }

    template <>
    inline VkObjectType GetObjectType<VkDeviceMemory>() { return VK_OBJECT_TYPE_DEVICE_MEMORY; }

    template <>
    inline VkObjectType GetObjectType<VkBuffer>() { return VK_OBJECT_TYPE_BUFFER; }

    template <>
    inline VkObjectType GetObjectType<VkImage>() { return VK_OBJECT_TYPE_IMAGE; }

    template <>
    inline VkObjectType GetObjectType<VkEvent>() { return VK_OBJECT_TYPE_EVENT; }

    template <>
    inline VkObjectType GetObjectType<VkQueryPool>() { return VK_OBJECT_TYPE_QUERY_POOL; }

    template <>
    inline VkObjectType GetObjectType<VkBufferView>() { return VK_OBJECT_TYPE_BUFFER_VIEW; }

    template <>
    inline VkObjectType GetObjectType<VkImageView>() { return VK_OBJECT_TYPE_IMAGE_VIEW; }

    template <>
    inline VkObjectType GetObjectType<VkShaderModule>() { return VK_OBJECT_TYPE_SHADER_MODULE; }

    template <>
    inline VkObjectType GetObjectType<VkPipelineCache>() { return VK_OBJECT_TYPE_PIPELINE_CACHE; }

    template <>
    inline VkObjectType GetObjectType<VkPipelineLayout>() { return VK_OBJECT_TYPE_PIPELINE_LAYOUT; }

    template <>
    inline VkObjectType GetObjectType<VkRenderPass>() { return VK_OBJECT_TYPE_RENDER_PASS; }

    template <>
    inline VkObjectType GetObjectType<VkPipeline>() { return VK_OBJECT_TYPE_PIPELINE; }

    template <>
    inline VkObjectType GetObjectType<VkDescriptorSetLayout>() { return VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT; }

    template <>
    inline VkObjectType GetObjectType<VkSampler>() { return VK_OBJECT_TYPE_SAMPLER; }

    template <>
    inline VkObjectType GetObjectType<VkDescriptorPool>() { return VK_OBJECT_TYPE_DESCRIPTOR_POOL; }

    template <>
    inline VkObjectType GetObjectType<VkDescriptorSet>() { return VK_OBJECT_TYPE_DESCRIPTOR_SET; }

    template <>
    inline VkObjectType GetObjectType<VkFramebuffer>() { return VK_OBJECT_TYPE_FRAMEBUFFER; }

    template <>
    inline VkObjectType GetObjectType<VkCommandPool>() { return VK_OBJECT_TYPE_COMMAND_POOL; }

    template <>
    inline VkObjectType GetObjectType<VkSurfaceKHR>() { return VK_OBJECT_TYPE_SURFACE_KHR; }

    template <>
    inline VkObjectType GetObjectType<VkSwapchainKHR>() { return VK_OBJECT_TYPE_SWAPCHAIN_KHR; }

    void SetHandleGPUName(Device& device, uint64_t handle, VkObjectType type, std::string_view name);

    template <typename Handle, typename... Args>
    inline void SetHandleGPUName(Device& device, Handle& handle, std::string_view fmt, Args&&... args)
    {
    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        char name[256] = { '\0' };
        sprintf_s(name, fmt.data(), std::forward<Args>(args)...);

        SetHandleGPUName(device, (uint64_t)handle.Get(), GetObjectType<typename Handle::Type>(), name);
    #endif
    }

    VkImageViewType ImageTypeToViewType(VkImageType type);
}
