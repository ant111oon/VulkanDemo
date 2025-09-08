#include "pch.h"

#include "vk_device.h"


namespace vkn
{
    static void CheckDeviceExtensionsSupport(VkPhysicalDevice vkPhysDevice, const std::span<const char* const> requiredExtensions)
    {
    #ifdef ENG_BUILD_DEBUG
        uint32_t vkDeviceExtensionsCount = 0;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(vkPhysDevice, nullptr, &vkDeviceExtensionsCount, nullptr));
        std::vector<VkExtensionProperties> vkDeviceExtensionProps(vkDeviceExtensionsCount);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(vkPhysDevice, nullptr, &vkDeviceExtensionsCount, vkDeviceExtensionProps.data()));

        bool areAllRequiredExtensionsAvailable = true;

        for (const char* pExtensionName : requiredExtensions) {
            const auto reqLayerIt = std::find_if(vkDeviceExtensionProps.cbegin(), vkDeviceExtensionProps.cend(), [&](const VkExtensionProperties& props) {
                return strcmp(pExtensionName, props.extensionName) == 0;
            });
            
            if (reqLayerIt == vkDeviceExtensionProps.cend()) {
                VK_LOG_ERROR("%s device extension is not supported", pExtensionName);
                areAllRequiredExtensionsAvailable = false;
            }
        }

        VK_ASSERT(areAllRequiredExtensionsAvailable);
    #endif
    }


    bool Device::Create(const DeviceCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Device is already initialized");
            return false;
        }

        VK_ASSERT(info.pPhysDevice && info.pPhysDevice->IsCreated());

        if (info.pSurface) {
            VK_ASSERT(info.pSurface->IsCreated());
        }

        CheckDeviceExtensionsSupport(info.pPhysDevice->Get(), info.extensions);

        m_pPhysDevice = info.pPhysDevice;

        uint32_t queueFamilyPropsCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_pPhysDevice->Get(), &queueFamilyPropsCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropsCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_pPhysDevice->Get(), &queueFamilyPropsCount, queueFamilyProps.data());

        uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
        uint32_t computeQueueFamilyIndex = UINT32_MAX;
        uint32_t transferQueueFamilyIndex = UINT32_MAX;

        auto IsQueueFamilyIndexValid = [](uint32_t index) -> bool { return index != UINT32_MAX; };

        for (uint32_t i = 0; i < queueFamilyProps.size(); ++i) {
            const VkQueueFamilyProperties& props = queueFamilyProps[i];

            if (info.pSurface) {
                VkBool32 isPresentSupported = VK_FALSE;
                VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(m_pPhysDevice->Get(), i, info.pSurface->Get(), &isPresentSupported));
                
                if (!isPresentSupported) {
                    continue;
                }
            }

            if (!IsQueueFamilyIndexValid(graphicsQueueFamilyIndex) && (props.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                graphicsQueueFamilyIndex = i;
            }

            if (!IsQueueFamilyIndexValid(computeQueueFamilyIndex) && (props.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                computeQueueFamilyIndex = i;
            }

            if (!IsQueueFamilyIndexValid(transferQueueFamilyIndex) && (props.queueFlags & VK_QUEUE_TRANSFER_BIT)) {
                transferQueueFamilyIndex = i;
            }

            if (IsQueueFamilyIndexValid(graphicsQueueFamilyIndex) && 
                IsQueueFamilyIndexValid(computeQueueFamilyIndex) && 
                IsQueueFamilyIndexValid(transferQueueFamilyIndex)
            ) {
                break;
            }
        }

        VK_ASSERT_MSG(IsQueueFamilyIndexValid(graphicsQueueFamilyIndex), "Failed to get graphics queue family index");
        VK_ASSERT_MSG(IsQueueFamilyIndexValid(computeQueueFamilyIndex),  "Failed to get compute queue family index");
        VK_ASSERT_MSG(IsQueueFamilyIndexValid(transferQueueFamilyIndex), "Failed to get transfer queue family index");

        VK_ASSERT_MSG(graphicsQueueFamilyIndex == computeQueueFamilyIndex && computeQueueFamilyIndex == transferQueueFamilyIndex,
            "Queue family indices for graphics, compute and transfer must be equal, for now. TODO: process the case when they are different");

        m_queueFamilyIndex = graphicsQueueFamilyIndex;

        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &info.queuePriority;

        VkDeviceCreateInfo deviceCreateInfo = {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = info.pFeatures2;
        deviceCreateInfo.pEnabledFeatures = info.pFeatures;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.enabledExtensionCount = info.extensions.size();
        deviceCreateInfo.ppEnabledExtensionNames = info.extensions.empty() ? nullptr : info.extensions.data();

        m_device = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDevice(m_pPhysDevice->Get(), &deviceCreateInfo, nullptr, &m_device));
        
        const bool isDeviceInitialized = m_device != VK_NULL_HANDLE;
        VK_ASSERT(isDeviceInitialized);
        
        vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);

        const bool isQueueInitialized = m_queue != VK_NULL_HANDLE;
        VK_ASSERT(isQueueInitialized);

        const bool isCreated = isDeviceInitialized && isQueueInitialized;

        m_state.set(FLAG_IS_CREATED, isCreated);

        return isCreated;
    }


    void Device::Destroy()
    {
        if (!IsCreated()) {
            return;
        }

        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;

        m_pPhysDevice = VK_NULL_HANDLE;

        m_queue = VK_NULL_HANDLE;

        m_state.reset();
    }
}