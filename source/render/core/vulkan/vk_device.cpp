#include "pch.h"

#include "vk_device.h"


namespace vkn
{
    static void CheckDeviceExtensionsSupport(VkPhysicalDevice vkPhysDevice, const std::span<const char* const> requiredExtensions)
    {
    #ifdef ENG_VK_DEBUG_UTILS_ENABLED
        uint32_t vkDeviceExtensionsCount = 0;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(vkPhysDevice, nullptr, &vkDeviceExtensionsCount, nullptr));
        std::vector<VkExtensionProperties> vkDeviceExtensionProps(vkDeviceExtensionsCount);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(vkPhysDevice, nullptr, &vkDeviceExtensionsCount, vkDeviceExtensionProps.data()));

        for (const char* pExtensionName : requiredExtensions) {
            const auto reqLayerIt = std::find_if(vkDeviceExtensionProps.cbegin(), vkDeviceExtensionProps.cend(), [&](const VkExtensionProperties& props) {
                return strcmp(pExtensionName, props.extensionName) == 0;
            });
            
            VK_ASSERT_MSG(reqLayerIt != vkDeviceExtensionProps.cend(), "\'%s\' device extension is not supported", pExtensionName);
        }
    #else
        (void)vkPhysDevice;
        (void)requiredExtensions;
    #endif
    }


    Device::~Device()
    {
        Destroy();
    }


    Device& Device::Create(const DeviceCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Recreation of Vulkan device");
            Destroy();
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
        VK_ASSERT(m_device != VK_NULL_HANDLE);
        
        vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);
        VK_ASSERT(m_queue != VK_NULL_HANDLE);

        SetCreated(true);

        return *this;
    }


    Device& Device::Destroy()
    {
        if (!IsCreated()) {
            return *this;
        }

        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;

        m_pPhysDevice = VK_NULL_HANDLE;

        m_queue = VK_NULL_HANDLE;

        Object::Destroy();

        return *this;
    }


    const Device& Device::WaitIdle() const
    {
        VK_ASSERT(IsCreated());
        VK_CHECK(vkDeviceWaitIdle(m_device));

        return *this;
    }


    PFN_vkVoidFunction Device::GetProcAddr(const char* pFuncName) const
    {
        VK_ASSERT(IsCreated());
        VK_ASSERT(pFuncName != nullptr);

        PFN_vkVoidFunction pFunc = vkGetDeviceProcAddr(m_device, pFuncName);
        VK_ASSERT_MSG(pFunc != nullptr, "Failed to load Vulkan function: %s", pFuncName);

        return pFunc;
    }
}