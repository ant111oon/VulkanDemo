#include "pch.h"

#include "vk_phys_device.h"


namespace vkn
{
    static bool IsPhysicalDeviceSuitable(
        VkPhysicalDevice vkPhysDevice,
        const PhysicalDevicePropertiesRequirenments& propsReq,
        VkPhysicalDeviceProperties2& outDeviceProps,
        VkPhysicalDeviceMemoryProperties& outMemoryProps,
        VkPhysicalDeviceFeatures2& outFeatures2
    ) {
        VK_ASSERT(vkPhysDevice != VK_NULL_HANDLE);
        
        vkGetPhysicalDeviceFeatures2(vkPhysDevice, &outFeatures2);
        vkGetPhysicalDeviceProperties2(vkPhysDevice, &outDeviceProps);
        vkGetPhysicalDeviceMemoryProperties(vkPhysDevice, &outMemoryProps);

        if (propsReq.deviceType != outDeviceProps.properties.deviceType) {
            return false;
        }

        return true;
    }


    PhysicalDevice::~PhysicalDevice()
    {
        Destroy();
    }


    PhysicalDevice& PhysicalDevice::Create(const PhysicalDeviceCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of Vulkan physical device");
            Destroy();
        }

        VK_ASSERT(info.pInstance && info.pInstance->IsCreated());
        VK_ASSERT(info.pPropertiesRequirenments);

        m_pInstance = info.pInstance;

        uint32_t physDeviceCount = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(m_pInstance->Get(), &physDeviceCount, nullptr));
        VK_ASSERT(physDeviceCount > 0);
        
        std::vector<VkPhysicalDevice> vkPhysDevices(physDeviceCount);
        VK_CHECK(vkEnumeratePhysicalDevices(m_pInstance->Get(), &physDeviceCount, vkPhysDevices.data()));

        bool isPicked = false;

        m_extendedDynStateFeatures = {};
        m_extendedDynStateFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;

        m_features13 = {};
        m_features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        m_features13.pNext = &m_extendedDynStateFeatures;

        m_features12 = {};
        m_features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        m_features12.pNext = &m_features13;

        m_features11 = {};
        m_features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        m_features11.pNext = &m_features12;

        m_features2 = {};
        m_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        m_features2.pNext = &m_features11;

        m_deviceDescBufferProps = {};
        m_deviceDescBufferProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;

        m_deviceProps = {};
        m_deviceProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        m_deviceProps.pNext = &m_deviceDescBufferProps;

        for (VkPhysicalDevice vkPhysDevice : vkPhysDevices) {
            if (IsPhysicalDeviceSuitable(vkPhysDevice, *info.pPropertiesRequirenments, m_deviceProps, m_memoryProps, m_features2)) {
                Base::Create([vkPhysDevice, &isPicked](VkPhysicalDevice& device) {
                    device = vkPhysDevice;
                    isPicked = device != VK_NULL_HANDLE;

                    return isPicked;
                });

                break;
            }
        }

        VK_ASSERT(isPicked);

        return *this;
    }


    PhysicalDevice& PhysicalDevice::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        m_pInstance = nullptr;

        m_memoryProps = {};
        m_deviceProps = {};
        m_deviceDescBufferProps = {};
        
        m_features13 = {};
        m_features12 = {};
        m_features11 = {};
        m_features2 = {};

        Base::Destroy([](VkPhysicalDevice& device) {
            device = VK_NULL_HANDLE;
        });

        return *this;
    }


    const VkPhysicalDeviceMemoryProperties& PhysicalDevice::GetMemoryProperties() const
    {
        VK_ASSERT(IsCreated());
        return m_memoryProps;
    }


    const VkPhysicalDeviceProperties2& PhysicalDevice::GetProperties() const
    {
        VK_ASSERT(IsCreated());
        return m_deviceProps;
    }


    const VkPhysicalDeviceDescriptorBufferPropertiesEXT& PhysicalDevice::GetDescBufferProperties() const
    {
        VK_ASSERT(IsCreated());
        return m_deviceDescBufferProps;
    }


    const VkPhysicalDeviceVulkan11Features& PhysicalDevice::GetFeatures11() const
    {
        VK_ASSERT(IsCreated());
        return m_features11;
    }


    const VkPhysicalDeviceVulkan12Features& PhysicalDevice::GetFeatures12() const
    {
        VK_ASSERT(IsCreated());
        return m_features12;
    }


    const VkPhysicalDeviceVulkan13Features& PhysicalDevice::GetFeatures13() const
    {
        VK_ASSERT(IsCreated());
        return m_features13;
    }


    const VkPhysicalDeviceFeatures2& PhysicalDevice::GetFeatures2() const
    {
        VK_ASSERT(IsCreated());
        return m_features2;
    }


    const Instance& PhysicalDevice::GetInstance() const
    {
        VK_ASSERT(IsCreated());
        return *m_pInstance;
    }


    Instance& PhysicalDevice::GetInstance()
    {
        VK_ASSERT(IsCreated());
        return *m_pInstance;
    }
}