#include "core/wnd_system/wnd_system.h"

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#ifdef ENG_OS_WINDOWS
    #include <vulkan/vulkan_win32.h>
#endif


#define VK_ASSERT_MSG(COND, FMT, ...) ENG_ASSERT_MSG(COND, "VULKAN", FMT, __VA_ARGS__)
#define VK_ASSERT(COND)               VK_ASSERT_MSG(COND, #COND)
#define VK_ASSERT_FAIL(FMT, ...)      VK_ASSERT_MSG(false, FMT, __VA_ARGS__)


#define VK_CHECK(VkCall)                                                                  \
    do {                                                                                  \
        const VkResult _vkCallResult = VkCall;                                            \
        VK_ASSERT_MSG(_vkCallResult == VK_SUCCESS, "%s", string_VkResult(_vkCallResult)); \
    } while(0)


bool CheckVkInstExtensionsSupport(const std::span<const char* const> requiredExtensions)
{
#ifdef ENG_BUILD_DEBUG
    uint32_t vkInstExtensionPropsCount = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &vkInstExtensionPropsCount, nullptr));
    std::vector<VkExtensionProperties> vkInstExtensionProps(vkInstExtensionPropsCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &vkInstExtensionPropsCount, vkInstExtensionProps.data()));

    for (const char* pExtensionName : requiredExtensions) {
        const auto reqExtIt = std::find_if(vkInstExtensionProps.cbegin(), vkInstExtensionProps.cend(), [&](const VkExtensionProperties& props) {
            return strcmp(pExtensionName, props.extensionName) == 0;
        });
        
        if (reqExtIt == vkInstExtensionProps.cend()) {
            return false;
        }
    }

    return true;
#else
    return true;
#endif
}


bool CheckVkInstLayersSupport(const std::span<const char* const> requiredLayers)
{
#ifdef ENG_BUILD_DEBUG
    uint32_t vkInstLayersPropsCount = 0;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&vkInstLayersPropsCount, nullptr));
    std::vector<VkLayerProperties> vkInstLayerProps(vkInstLayersPropsCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&vkInstLayersPropsCount, vkInstLayerProps.data()));

    for (const char* pLayerName : requiredLayers) {
        const auto reqLayerIt = std::find_if(vkInstLayerProps.cbegin(), vkInstLayerProps.cend(), [&](const VkLayerProperties& props) {
            return strcmp(pLayerName, props.layerName) == 0;
        });
        
        if (reqLayerIt == vkInstLayerProps.cend()) {
            return false;
        }
    }

    return true;
#else
    return true;
#endif
}


bool CheckVkDeviceExtensionsSupport(VkPhysicalDevice vkPhysDevice, const std::span<const char* const> requiredExtensions)
{
#ifdef ENG_BUILD_DEBUG
    uint32_t vkDeviceExtensionsCount = 0;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(vkPhysDevice, nullptr, &vkDeviceExtensionsCount, nullptr));
    std::vector<VkExtensionProperties> vkDeviceExtensionProps(vkDeviceExtensionsCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(vkPhysDevice, nullptr, &vkDeviceExtensionsCount, vkDeviceExtensionProps.data()));

    for (const char* pExtensionName : requiredExtensions) {
        const auto reqLayerIt = std::find_if(vkDeviceExtensionProps.cbegin(), vkDeviceExtensionProps.cend(), [&](const VkExtensionProperties& props) {
            return strcmp(pExtensionName, props.extensionName) == 0;
        });
        
        if (reqLayerIt == vkDeviceExtensionProps.cend()) {
            return false;
        }
    }

    return true;
#else
    return true;
#endif
}


#ifdef ENG_BUILD_DEBUG
static VkBool32 VKAPI_PTR DbgVkMessageCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
) {
    if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        return VK_FALSE;
    }

    const char* pType = "UNKNOWN TYPE";

    switch(messageTypes) {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            pType = "GENERAL";
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            pType = "VALIDATION";
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            pType = "PERFORMANCE";
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT:
            pType = "DEVICE ADDR BINDING";
            break;
        default:
            VK_ASSERT_FAIL("Invalid message type");
            break;
    }

    switch(messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            ENG_LOG_TRACE("VULKAN", "[%s]: %s", pType, pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            ENG_LOG_INFO("VULKAN", "[%s]: %s", pType, pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            ENG_LOG_WARN("VULKAN", "[%s]: %s", pType, pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            VK_ASSERT_MSG(false, "[%s]: %s", pType, pCallbackData->pMessage);
            break;
        default:
            VK_ASSERT_FAIL("Invalid message severity");
            break;
    }

    return VK_FALSE;
}
#endif


VkInstance InitVkInstance(const char* pAppName)
{
    VkApplicationInfo vkApplicationInfo = {};
    vkApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    vkApplicationInfo.pApplicationName = pAppName;
    vkApplicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    vkApplicationInfo.pEngineName = "VkEngine";
    vkApplicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    vkApplicationInfo.apiVersion = VK_API_VERSION_1_3;

    constexpr std::array vkInstExtensions = {
    #ifdef ENG_BUILD_DEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    #endif

        VK_KHR_SURFACE_EXTENSION_NAME,
    #ifdef ENG_OS_WINDOWS
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    #endif
    };

    VK_ASSERT_MSG(CheckVkInstExtensionsSupport(vkInstExtensions), "Not all required instance extensions are supported");

#ifdef ENG_BUILD_DEBUG
     constexpr std::array vkInstLayers = {
        "VK_LAYER_KHRONOS_validation",
    };
    
    VK_ASSERT_MSG(CheckVkInstLayersSupport(vkInstLayers), "Not all required instance layers are supported");

    VkDebugUtilsMessengerCreateInfoEXT vkDbgMessengerCreateInfo = {};
    vkDbgMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    vkDbgMessengerCreateInfo.messageType = 
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;
    vkDbgMessengerCreateInfo.messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    vkDbgMessengerCreateInfo.pfnUserCallback = DbgVkMessageCallback;
#endif

    VkInstanceCreateInfo vkInstCreateInfo = {};
    vkInstCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    vkInstCreateInfo.pApplicationInfo = &vkApplicationInfo;
    
#ifdef ENG_BUILD_DEBUG
    vkInstCreateInfo.pNext = &vkDbgMessengerCreateInfo;
    vkInstCreateInfo.enabledLayerCount = vkInstLayers.size();
    vkInstCreateInfo.ppEnabledLayerNames = vkInstLayers.data();
#else
    vkInstCreateInfo.enabledLayerCount = 0;
    vkInstCreateInfo.ppEnabledLayerNames = nullptr;
#endif

    vkInstCreateInfo.enabledExtensionCount = vkInstExtensions.size();
    vkInstCreateInfo.ppEnabledExtensionNames = vkInstExtensions.data();

    VkInstance vkInstance = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&vkInstCreateInfo, nullptr, &vkInstance));
    VK_ASSERT(vkInstance != VK_NULL_HANDLE);

    return vkInstance;
}


VkSurfaceKHR InitVkSurface(VkInstance vkInstance, const BaseWindow& wnd)
{
    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;

#ifdef ENG_OS_WINDOWS
    VkWin32SurfaceCreateInfoKHR vkWin32SurfCreateInfo = {};
    vkWin32SurfCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    vkWin32SurfCreateInfo.hinstance = GetModuleHandle(nullptr);
    vkWin32SurfCreateInfo.hwnd = (HWND)wnd.GetNativeHandle();

    VK_CHECK(vkCreateWin32SurfaceKHR(vkInstance, &vkWin32SurfCreateInfo, nullptr, &vkSurface));
#endif

    VK_ASSERT(vkSurface != VK_NULL_HANDLE);

    return vkSurface;
}


VkPhysicalDevice InitVkPhysDevice(VkInstance vkInstance)
{
    uint32_t physDeviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(vkInstance, &physDeviceCount, nullptr));
    VK_ASSERT(physDeviceCount > 0);
    
    std::vector<VkPhysicalDevice> physDevices(physDeviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(vkInstance, &physDeviceCount, physDevices.data()));

    VkPhysicalDevice pickedPhysDevice = VK_NULL_HANDLE;

    for (VkPhysicalDevice device : physDevices) {
        bool isDeviceSuitable = true;

        VkPhysicalDeviceFeatures features = {};
        vkGetPhysicalDeviceFeatures(device, &features);

        isDeviceSuitable = isDeviceSuitable && features.independentBlend;

        VkPhysicalDeviceProperties props = {};
        vkGetPhysicalDeviceProperties(device, &props);

        isDeviceSuitable = isDeviceSuitable && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

        if (isDeviceSuitable) {
            pickedPhysDevice = device;
            break;
        }
    }

    VK_ASSERT(pickedPhysDevice != VK_NULL_HANDLE);

    return pickedPhysDevice;
}


VkDevice InitVkDevice(VkPhysicalDevice vkPhysDevice, VkSurfaceKHR vkSurface)
{
    uint32_t queueFamilyPropsCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vkPhysDevice, &queueFamilyPropsCount, nullptr);
    
    std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropsCount);
    vkGetPhysicalDeviceQueueFamilyProperties(vkPhysDevice, &queueFamilyPropsCount, queueFamilyProps.data());

    uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
    uint32_t computeQueueFamilyIndex = UINT32_MAX;
    uint32_t transferQueueFamilyIndex = UINT32_MAX;

    auto IsQueueFamilyIndexValid = [](uint32_t index) -> bool { return index != UINT32_MAX; };

    for (uint32_t i = 0; i < queueFamilyProps.size(); ++i) {
        const VkQueueFamilyProperties& props = queueFamilyProps[i];

        VkBool32 isPresentSupported = VK_FALSE;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysDevice, i, vkSurface, &isPresentSupported));
        if (!isPresentSupported) {
            continue;
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
    VK_ASSERT_MSG(IsQueueFamilyIndexValid(computeQueueFamilyIndex), "Failed to get compute queue family index");
    VK_ASSERT_MSG(IsQueueFamilyIndexValid(transferQueueFamilyIndex), "Failed to get transfer queue family index");

    VK_ASSERT_MSG(graphicsQueueFamilyIndex == computeQueueFamilyIndex && computeQueueFamilyIndex == transferQueueFamilyIndex,
        "Queue family indices for graphics, compute and transfer must be equal, for now. TODO: process the case when they are different");

    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    
    const float queuePriority = 1.f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

    constexpr std::array deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
    };

    VK_ASSERT(CheckVkDeviceExtensionsSupport(vkPhysDevice, deviceExtensions));

    deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkPhysicalDeviceDynamicRenderingFeatures dynRenderingFeature = {};
    dynRenderingFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynRenderingFeature.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &dynRenderingFeature;

    deviceCreateInfo.pNext = &features2;

    VkDevice device = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(vkPhysDevice, &deviceCreateInfo, nullptr, &device));

    return device;
}


int main(int argc, char* argv[])
{
    wndSysInit();
    BaseWindow* pWnd = wndSysGetMainWindow();

    WindowInitInfo wndInitInfo = {};
    wndInitInfo.title = "Vulkan Demo";
    wndInitInfo.width = 980;
    wndInitInfo.height = 640;

    pWnd->Init(wndInitInfo);
    ENG_ASSERT(pWnd->IsInitialized());

    VkInstance vkInstance = InitVkInstance(wndInitInfo.title.data());
    VkSurfaceKHR vkSurface = InitVkSurface(vkInstance, *pWnd);
    VkPhysicalDevice vkPhysDevice = InitVkPhysDevice(vkInstance);
    VkDevice vkDevice = InitVkDevice(vkPhysDevice, vkSurface);

    while(!pWnd->IsClosed()) {
        pWnd->ProcessEvents();
        
        WndEvent event;
        while(pWnd->PopEvent(event)) {
        }
    }

    VK_CHECK(vkDeviceWaitIdle(vkDevice));

    vkDestroyDevice(vkDevice, nullptr);
    vkDestroySurfaceKHR(vkInstance, vkSurface, nullptr);
    
    // NOTE: Debug messenger is automatically created and destroyed by the Vulkan loader via pNext.
    vkDestroyInstance(vkInstance, nullptr);

    pWnd->Destroy();
    wndSysTerminate();

    return 0;
}