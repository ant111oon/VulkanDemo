#include "pch.h"

#include "vk_instance.h"
#include "vk_core.h"


namespace vkn
{
    static void CheckInstanceExtensionsSupport(const std::span<const char* const> requiredExtensions)
    {
    #ifdef ENG_BUILD_DEBUG
        uint32_t vkInstExtensionPropsCount = 0;
        VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &vkInstExtensionPropsCount, nullptr));
        std::vector<VkExtensionProperties> vkInstExtensionProps(vkInstExtensionPropsCount);
        VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &vkInstExtensionPropsCount, vkInstExtensionProps.data()));

        bool areAllRequiredExtensionsAvailable = true;

        for (const char* pExtensionName : requiredExtensions) {
            const auto reqExtIt = std::find_if(vkInstExtensionProps.cbegin(), vkInstExtensionProps.cend(), [&](const VkExtensionProperties& props) {
                return strcmp(pExtensionName, props.extensionName) == 0;
            });
            
            if (reqExtIt == vkInstExtensionProps.cend()) {
                VK_LOG_ERROR("%s instance extension is not supported", pExtensionName);
                areAllRequiredExtensionsAvailable = false;
            }
        }

        VK_ASSERT(areAllRequiredExtensionsAvailable);
    #endif
    }


    static void CheckInstanceLayersSupport(const std::span<const char* const> requiredLayers)
    {
    #ifdef ENG_BUILD_DEBUG
        uint32_t vkInstLayersPropsCount = 0;
        VK_CHECK(vkEnumerateInstanceLayerProperties(&vkInstLayersPropsCount, nullptr));
        std::vector<VkLayerProperties> vkInstLayerProps(vkInstLayersPropsCount);
        VK_CHECK(vkEnumerateInstanceLayerProperties(&vkInstLayersPropsCount, vkInstLayerProps.data()));

        bool areAllRequiredLayersAvailable = true;

        for (const char* pLayerName : requiredLayers) {
            const auto reqLayerIt = std::find_if(vkInstLayerProps.cbegin(), vkInstLayerProps.cend(), [&](const VkLayerProperties& props) {
                return strcmp(pLayerName, props.layerName) == 0;
            });
            
            if (reqLayerIt == vkInstLayerProps.cend()) {
                VK_LOG_ERROR("%s instance layer is not supported", pLayerName);
                areAllRequiredLayersAvailable = false;
            }
        }

        VK_ASSERT(areAllRequiredLayersAvailable);
    #endif
    }


    static VkDebugUtilsMessengerEXT CreateDebugMessenger(VkInstance vkInstance, const VkDebugUtilsMessengerCreateInfoEXT& vkDbgMessengerCreateInfo)
    {
        VK_ASSERT(vkInstance != VK_NULL_HANDLE);

        VkDebugUtilsMessengerEXT vkDbgUtilsMessenger = VK_NULL_HANDLE;

        auto CreateDebugUtilsMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkInstance, "vkCreateDebugUtilsMessengerEXT");
        VK_ASSERT(CreateDebugUtilsMessenger);

        VK_CHECK(CreateDebugUtilsMessenger(vkInstance, &vkDbgMessengerCreateInfo, nullptr, &vkDbgUtilsMessenger));
        VK_ASSERT(vkDbgUtilsMessenger != VK_NULL_HANDLE);

        CreateDebugUtilsMessenger = nullptr;

        return vkDbgUtilsMessenger;
    }


    static void DestroyDebugMessenger(VkInstance vkInstance, VkDebugUtilsMessengerEXT& vkDbgUtilsMessenger)
    {
        VK_ASSERT(vkInstance != VK_NULL_HANDLE);

        if (vkDbgUtilsMessenger == VK_NULL_HANDLE) {
            return;
        }

        auto DestroyDebugUtilsMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkInstance, "vkDestroyDebugUtilsMessengerEXT");
        VK_ASSERT(DestroyDebugUtilsMessenger);

        DestroyDebugUtilsMessenger(vkInstance, vkDbgUtilsMessenger, nullptr);
        vkDbgUtilsMessenger = VK_NULL_HANDLE;

        DestroyDebugUtilsMessenger = nullptr;
    }


    bool Instance::Create(const InstanceCreateInfo& info)
    {
        if (IsCreated()) {
            VK_LOG_WARN("Instance is already created");
            return true;
        }

        CheckInstanceExtensionsSupport(info.extensions);
        CheckInstanceLayersSupport(info.layers);

        VkApplicationInfo vkApplicationInfo = {};
        vkApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        vkApplicationInfo.pApplicationName = info.pApplicationName;
        vkApplicationInfo.applicationVersion = info.applicationVersion;
        vkApplicationInfo.pEngineName = info.pEngineName;
        vkApplicationInfo.engineVersion = info.engineVersion;
        vkApplicationInfo.apiVersion = info.apiVersion;

        const bool dbgMessengerEnabled = info.pDbgMessengerCreateInfo != nullptr;

        VkDebugUtilsMessengerCreateInfoEXT vkDbgMessengerCreateInfo = {};
        if (dbgMessengerEnabled) {
            vkDbgMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            vkDbgMessengerCreateInfo.messageType = info.pDbgMessengerCreateInfo->messageType;
            vkDbgMessengerCreateInfo.messageSeverity = info.pDbgMessengerCreateInfo->messageSeverity;
            vkDbgMessengerCreateInfo.pfnUserCallback = info.pDbgMessengerCreateInfo->pMessageCallback;
            vkDbgMessengerCreateInfo.pUserData = info.pDbgMessengerCreateInfo->pUserData;
            vkDbgMessengerCreateInfo.flags = info.pDbgMessengerCreateInfo->flags;
        }

        VkInstanceCreateInfo vkInstCreateInfo = {};
        vkInstCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        vkInstCreateInfo.pApplicationInfo = &vkApplicationInfo;
        
        if (dbgMessengerEnabled) {
            vkInstCreateInfo.pNext = &vkDbgMessengerCreateInfo;    
        }

        vkInstCreateInfo.enabledLayerCount = info.layers.size();
        vkInstCreateInfo.ppEnabledLayerNames = info.layers.empty() ? nullptr : info.layers.data();
        vkInstCreateInfo.enabledExtensionCount = info.extensions.size();
        vkInstCreateInfo.ppEnabledExtensionNames = info.extensions.empty() ? nullptr : info.extensions.data();

        m_instance = VK_NULL_HANDLE;
        VK_CHECK(vkCreateInstance(&vkInstCreateInfo, nullptr, &m_instance));
        VK_ASSERT(m_instance != VK_NULL_HANDLE);

        if (dbgMessengerEnabled) {
            m_dbgMessenger = CreateDebugMessenger(m_instance, vkDbgMessengerCreateInfo);
        }

        m_flags.set(FLAG_IS_CREATED, true);

        return true;
    }


    void Instance::Destroy()
    {
        if (!IsCreated()) {
            return;
        }

        DestroyDebugMessenger(m_instance, m_dbgMessenger);
        
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;

        m_flags.set(FLAG_IS_CREATED, false);
    }
}