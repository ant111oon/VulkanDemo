#include "pch.h"

#include "vk_phys_device.h"


namespace vkn
{
    static bool IsPhysicalDeviceSuitable(
        VkPhysicalDevice vkPhysDevice, 
        const PhysicalDeviceFeaturesRequirenments& featuresReq, 
        const PhysicalDevicePropertiesRequirenments& propsReq
    ) {
        VkPhysicalDeviceFeatures features = {};
        vkGetPhysicalDeviceFeatures(vkPhysDevice, &features);

        if (featuresReq.independentBlend.has_value() && featuresReq.independentBlend.value() != features.independentBlend) {
            return false;
        }

        VkPhysicalDeviceProperties deviceProps = {};
        vkGetPhysicalDeviceProperties(vkPhysDevice, &deviceProps);

        if (propsReq.deviceType != deviceProps.deviceType) {
            return false;
        }

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

        for (VkPhysicalDevice vkPhysDevice : vkPhysDevices) {
            if (IsPhysicalDeviceSuitable(vkPhysDevice, *info.pFeaturesRequirenments, *info.pPropertiesRequirenments)) {
                m_physDevice = vkPhysDevice;
                vkGetPhysicalDeviceProperties(m_physDevice, &m_deviceProps);
                vkGetPhysicalDeviceFeatures(m_physDevice, &m_features);
                vkGetPhysicalDeviceMemoryProperties(m_physDevice, &m_memoryProps);

                isPicked = true;
                break;
            }
        }

        VK_ASSERT(isPicked);
        m_state.set(FLAG_IS_CREATED, isPicked);

        return isPicked;
    }


    void PhysicalDevice::Destroy()
    {
        m_physDevice = VK_NULL_HANDLE;
        m_pInstance = nullptr;

        m_memoryProps = {};
        m_deviceProps = {};
        m_features = {};

        m_state.reset();
    }
}