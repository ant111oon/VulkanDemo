#pragma once

#include "vk_object.h"
#include "vk_instance.h"


namespace vkn
{
    struct PhysicalDeviceFeaturesRequirenments
    {
        std::optional<bool> independentBlend;
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

        VkPhysicalDeviceMemoryProperties GetMemoryProperties() const
        {
            VK_ASSERT(IsCreated());
            return m_memoryProps;
        }

        VkPhysicalDeviceProperties GetProperties() const
        {
            VK_ASSERT(IsCreated());
            return m_deviceProps;
        }

        VkPhysicalDeviceFeatures GetFeatures() const
        {
            VK_ASSERT(IsCreated());
            return m_features;
        }

    private:
        PhysicalDevice() = default;

    private:
        Instance* m_pInstance = nullptr;
        VkPhysicalDevice m_physDevice = VK_NULL_HANDLE;

        VkPhysicalDeviceMemoryProperties m_memoryProps = {};
        VkPhysicalDeviceProperties m_deviceProps = {};
        VkPhysicalDeviceFeatures m_features = {};
    };


    ENG_FORCE_INLINE PhysicalDevice& GetPhysicalDevice()
    {
        static PhysicalDevice device;
        return device;
    }
}