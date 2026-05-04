#pragma once

#include "vk_instance.h"


namespace vkn
{
    struct PhysicalDeviceFeaturesRequirenments
    {
        bool independentBlend;
        bool descriptorBindingPartiallyBound;
        bool runtimeDescriptorArray;
        bool descriptorIndexing;
        bool samplerAnisotropy;
        bool samplerMirrorClampToEdge;
        bool drawIndirectCount;
        bool vertexPipelineStoresAndAtomics;
        bool bufferDeviceAddress;
        bool bufferDeviceAddressCaptureReplay;
        bool shaderFloat16;
    };


    struct PhysicalDevicePropertiesRequirenments
    {
        VkPhysicalDeviceType deviceType;
    };


    struct PhysicalDeviceCreateInfo
    {
        Instance* pInstance;
        const PhysicalDeviceFeaturesRequirenments* pFeaturesRequirenments;
        const PhysicalDevicePropertiesRequirenments* pPropertiesRequirenments;
    };


    class PhysicalDevice : public Handle<VkPhysicalDevice>
    {
        friend PhysicalDevice& GetPhysicalDevice();

    public:
        using Base = Handle<VkPhysicalDevice>;

    public:
        ENG_DECL_CLASS_NO_COPIABLE(PhysicalDevice);
        ENG_DECL_CLASS_NO_MOVABLE(PhysicalDevice);

        ~PhysicalDevice();

        PhysicalDevice& Create(const PhysicalDeviceCreateInfo& info);
        PhysicalDevice& Destroy();

        const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const;

        const VkPhysicalDeviceProperties2& GetProperties() const;

        const VkPhysicalDeviceDescriptorBufferPropertiesEXT& GetDescBufferProperties() const;

        const VkPhysicalDeviceVulkan11Features& GetFeatures11() const;
        const VkPhysicalDeviceVulkan12Features& GetFeatures12() const;
        const VkPhysicalDeviceVulkan13Features& GetFeatures13() const;
        const VkPhysicalDeviceFeatures2& GetFeatures2() const;

        const Instance& GetInstance() const;
        Instance& GetInstance();

    private:
        PhysicalDevice() = default;

    private:
        Instance* m_pInstance = nullptr;

        VkPhysicalDeviceMemoryProperties m_memoryProps = {};
        VkPhysicalDeviceProperties2 m_deviceProps = {};
        VkPhysicalDeviceDescriptorBufferPropertiesEXT m_deviceDescBufferProps = {};

        VkPhysicalDeviceVulkan13Features m_features13 = {};
        VkPhysicalDeviceVulkan12Features m_features12 = {};
        VkPhysicalDeviceVulkan11Features m_features11 = {};
        VkPhysicalDeviceFeatures2 m_features2 = {};
    };


    ENG_FORCE_INLINE PhysicalDevice& GetPhysicalDevice()
    {
        static PhysicalDevice device;
        return device;
    }
}