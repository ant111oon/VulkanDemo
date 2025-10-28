#pragma once

#include "vk_object.h"
#include "vk_instance.h"


namespace vkn
{
    struct PhysicalDeviceFeaturesRequirenments
    {
        bool independentBlend;
        bool descriptorBindingPartiallyBound;
        bool runtimeDescriptorArray;
        bool samplerAnisotropy;
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


    class PhysicalDevice : public Object
    {
        friend PhysicalDevice& GetPhysicalDevice();

    public:
        ENG_DECL_CLASS_NO_COPIABLE(PhysicalDevice);
        ENG_DECL_CLASS_NO_MOVABLE(PhysicalDevice);

        bool Create(const PhysicalDeviceCreateInfo& info);
        void Destroy();

        VkPhysicalDevice Get() const
        {
            VK_ASSERT(IsCreated());
            return m_physDevice;
        }

        const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const
        {
            VK_ASSERT(IsCreated());
            return m_memoryProps;
        }

        const VkPhysicalDeviceProperties& GetProperties() const
        {
            VK_ASSERT(IsCreated());
            return m_deviceProps;
        }

        const VkPhysicalDeviceVulkan11Features& GetFeatures11() const
        {
            VK_ASSERT(IsCreated());
            return m_features11;
        }

        const VkPhysicalDeviceVulkan12Features& GetFeatures12() const
        {
            VK_ASSERT(IsCreated());
            return m_features12;
        }

        const VkPhysicalDeviceVulkan13Features& GetFeatures13() const
        {
            VK_ASSERT(IsCreated());
            return m_features13;
        }

        const VkPhysicalDeviceFeatures2& GetFeatures2() const
        {
            VK_ASSERT(IsCreated());
            return m_features2;
        }

        const Instance* GetInstance() const
        {
            VK_ASSERT(IsCreated());
            return m_pInstance;
        }

        Instance* GetInstance()
        {
            VK_ASSERT(IsCreated());
            return m_pInstance;
        }

    private:
        PhysicalDevice() = default;

    private:
        Instance* m_pInstance = nullptr;
        VkPhysicalDevice m_physDevice = VK_NULL_HANDLE;

        VkPhysicalDeviceMemoryProperties m_memoryProps = {};
        VkPhysicalDeviceProperties m_deviceProps = {};

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