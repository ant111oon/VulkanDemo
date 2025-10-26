#include "pch.h"

#include "vk_phys_device.h"


namespace vkn
{
    static bool IsPhysicalDeviceSuitable(
        VkPhysicalDevice vkPhysDevice, 
        const PhysicalDeviceFeaturesRequirenments& featuresReq, 
        const PhysicalDevicePropertiesRequirenments& propsReq,
        VkPhysicalDeviceProperties& outDeviceProps,
        VkPhysicalDeviceMemoryProperties& outMemoryProps,
        VkPhysicalDeviceFeatures2& outFeatures2
    ) {
        VK_ASSERT(vkPhysDevice != VK_NULL_HANDLE);
        
        vkGetPhysicalDeviceFeatures2(vkPhysDevice, &outFeatures2);        

        if (featuresReq.independentBlend && featuresReq.independentBlend != outFeatures2.features.independentBlend) {
            return false;
        }

        const auto pFeatures11 = static_cast<VkPhysicalDeviceVulkan11Features*>(outFeatures2.pNext);
        const auto pFeatures12 = static_cast<VkPhysicalDeviceVulkan12Features*>(pFeatures11->pNext);
        const auto pFeatures13 = static_cast<VkPhysicalDeviceVulkan13Features*>(pFeatures12->pNext);

        if (featuresReq.descriptorBindingPartiallyBound && featuresReq.descriptorBindingPartiallyBound != pFeatures12->descriptorBindingPartiallyBound) {
            return false;
        }

        if (featuresReq.runtimeDescriptorArray && featuresReq.runtimeDescriptorArray != pFeatures12->runtimeDescriptorArray) {
            return false;
        }

        vkGetPhysicalDeviceProperties(vkPhysDevice, &outDeviceProps);

        if (propsReq.deviceType != outDeviceProps.deviceType) {
            return false;
        }

        vkGetPhysicalDeviceMemoryProperties(vkPhysDevice, &outMemoryProps);

        return true;
    }


    bool PhysicalDevice::Create(const PhysicalDeviceCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("PhysicalDevice is already initialized");
            return false;
        }

        VK_ASSERT(info.pInstance && info.pInstance->IsCreated());
        VK_ASSERT(info.pFeaturesRequirenments);
        VK_ASSERT(info.pPropertiesRequirenments);

        m_pInstance = info.pInstance;

        uint32_t physDeviceCount = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(m_pInstance->Get(), &physDeviceCount, nullptr));
        VK_ASSERT(physDeviceCount > 0);
        
        std::vector<VkPhysicalDevice> vkPhysDevices(physDeviceCount);
        VK_CHECK(vkEnumeratePhysicalDevices(m_pInstance->Get(), &physDeviceCount, vkPhysDevices.data()));

        bool isPicked = false;

        m_features13 = {};
        m_features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        m_features12 = {};
        m_features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        m_features12.pNext = &m_features13;

        m_features11 = {};
        m_features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        m_features11.pNext = &m_features12;

        m_features2 = {};
        m_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        m_features2.pNext = &m_features11;

        for (VkPhysicalDevice vkPhysDevice : vkPhysDevices) {
            if (IsPhysicalDeviceSuitable(
                vkPhysDevice, 
                *info.pFeaturesRequirenments, 
                *info.pPropertiesRequirenments, 
                m_deviceProps,
                m_memoryProps,
                m_features2)
            ) {
                m_physDevice = vkPhysDevice;
                isPicked = true;

                break;
            }
        }

        VK_ASSERT(isPicked);

        SetCreated(isPicked);

        return isPicked;
    }


    void PhysicalDevice::Destroy()
    {
        if (!IsCreated()) {
            return;
        }

        m_physDevice = VK_NULL_HANDLE;
        m_pInstance = nullptr;

        m_memoryProps = {};
        m_deviceProps = {};
        
        m_features13 = {};
        m_features12 = {};
        m_features11 = {};
        m_features2 = {};

        Object::Destroy();
    }
}