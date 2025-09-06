#pragma once

#include "vk_core.h"

#include <bitset>


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
        VkInstance* pInstance;
        const PhysicalDeviceFeaturesRequirenments* pFeaturesRequirenments;
        const PhysicalDevicePropertiesRequirenments* pPropertiesRequirenments;
    };


    class PhysicalDevice
    {
        friend PhysicalDevice& GetPhysicalDevice();

    public:
        PhysicalDevice(const PhysicalDevice& device) = delete;
        PhysicalDevice(PhysicalDevice&& device) = delete;

        PhysicalDevice& operator=(const PhysicalDevice& device) = delete;
        PhysicalDevice& operator=(PhysicalDevice&& device) = delete;

        bool Create(const PhysicalDeviceCreateInfo& info);
        void Destroy();

        VkPhysicalDevice& Get()
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

        const VkPhysicalDeviceFeatures& GetFeatures() const
        {
            VK_ASSERT(IsCreated());
            return m_features;
        }

        bool IsCreated() const { return m_flags.test(FLAG_IS_CREATED); }

    private:
        PhysicalDevice() = default;

    private:
        enum
        {
            FLAG_IS_CREATED,
            FLAG_COUNT,
        };

    private:
        VkInstance* m_pInstance = nullptr;
        VkPhysicalDevice m_physDevice = VK_NULL_HANDLE;

        VkPhysicalDeviceMemoryProperties m_memoryProps = {};
        VkPhysicalDeviceProperties m_deviceProps = {};
        VkPhysicalDeviceFeatures m_features = {};

        std::bitset<FLAG_COUNT> m_flags = {};
    };


    ENG_FORCE_INLINE PhysicalDevice& GetPhysicalDevice()
    {
        static PhysicalDevice device;
        return device;
    }
}