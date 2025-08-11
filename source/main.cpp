#include "core/wnd_system/wnd_system.h"

#include "core/platform/file/file.h"

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#ifdef ENG_OS_WINDOWS
    #include <vulkan/vulkan_win32.h>
#endif


namespace fs = std::filesystem;


#define VK_LOG_INFO(FMT, ...)         ENG_LOG_INFO("VULKAN", FMT, __VA_ARGS__)
#define VK_ASSERT_MSG(COND, FMT, ...) ENG_ASSERT_MSG(COND, "VULKAN", FMT, __VA_ARGS__)
#define VK_ASSERT(COND)               VK_ASSERT_MSG(COND, #COND)
#define VK_ASSERT_FAIL(FMT, ...)      VK_ASSERT_MSG(false, FMT, __VA_ARGS__)


#define VK_CHECK(VkCall)                                                                  \
    do {                                                                                  \
        const VkResult _vkCallResult = VkCall;                                            \
        (void)_vkCallResult;                                                              \
        VK_ASSERT_MSG(_vkCallResult == VK_SUCCESS, "%s", string_VkResult(_vkCallResult)); \
    } while(0)


static VkSurfaceFormatKHR s_swapchainSurfFormat = {};


class Timer
{
public:
    Timer()
    {
        Start();
    }

    Timer& Reset()
    { 
        m_start = m_end = std::chrono::high_resolution_clock::now();
        return *this;
    }

    Timer& Start()
    {
        m_start = std::chrono::high_resolution_clock::now();
        return *this;
    }

    Timer& End()
    {
        m_end = std::chrono::high_resolution_clock::now();
        return *this;
    }


    template<typename DURATION_T, typename PERIOD_T>
    DURATION_T GetDuration() const
    {
        ENG_ASSERT_MSG(m_end > m_start, "CORE", "Need to call End() before GetDuration()");
        return std::chrono::duration<DURATION_T, PERIOD_T>(m_end - m_start).count();
    }

private:
    using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

    TimePoint m_start;
    TimePoint m_end;
};


static bool CheckVkInstExtensionsSupport(const std::span<const char* const> requiredExtensions)
{
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
}


static bool CheckVkInstLayersSupport(const std::span<const char* const> requiredLayers)
{
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
}


static bool CheckVkDeviceExtensionsSupport(VkPhysicalDevice vkPhysDevice, const std::span<const char* const> requiredExtensions)
{
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


static VkDebugUtilsMessengerEXT InitVkDebugMessenger(VkInstance vkInstance, const VkDebugUtilsMessengerCreateInfoEXT& vkDbgMessengerCreateInfo)
{
    VkDebugUtilsMessengerEXT vkDbgUtilsMessenger = VK_NULL_HANDLE;

    auto CreateDebugUtilsMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkInstance, "vkCreateDebugUtilsMessengerEXT");
    VK_ASSERT(CreateDebugUtilsMessenger);

    VK_CHECK(CreateDebugUtilsMessenger(vkInstance, &vkDbgMessengerCreateInfo, nullptr, &vkDbgUtilsMessenger));
    VK_ASSERT(vkDbgUtilsMessenger != VK_NULL_HANDLE);

    CreateDebugUtilsMessenger = nullptr;

    return vkDbgUtilsMessenger;
}


static void DestroyVkDebugMessenger(VkInstance vkInstance, VkDebugUtilsMessengerEXT& vkDbgUtilsMessenger)
{
    if (vkDbgUtilsMessenger == VK_NULL_HANDLE) {
        return;
    }

    auto DestroyDebugUtilsMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkInstance, "vkDestroyDebugUtilsMessengerEXT");
    VK_ASSERT(DestroyDebugUtilsMessenger);

    DestroyDebugUtilsMessenger(vkInstance, vkDbgUtilsMessenger, nullptr);
    vkDbgUtilsMessenger = VK_NULL_HANDLE;

    DestroyDebugUtilsMessenger = nullptr;
}


static VkInstance InitVkInstance(const char* pAppName, VkDebugUtilsMessengerEXT& vkDbgUtilsMessenger)
{
    Timer timer;

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

#ifdef ENG_BUILD_DEBUG
    vkDbgUtilsMessenger = InitVkDebugMessenger(vkInstance, vkDbgMessengerCreateInfo);
#else
    vkDbgUtilsMessenger = VK_NULL_HANDLE;
#endif

    VK_LOG_INFO("VkInstance initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkInstance;
}


static VkSurfaceKHR InitVkSurface(VkInstance vkInstance, const BaseWindow& wnd)
{
    Timer timer;

    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;

#ifdef ENG_OS_WINDOWS
    VkWin32SurfaceCreateInfoKHR vkWin32SurfCreateInfo = {};
    vkWin32SurfCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    vkWin32SurfCreateInfo.hinstance = GetModuleHandle(nullptr);
    vkWin32SurfCreateInfo.hwnd = (HWND)wnd.GetNativeHandle();

    VK_CHECK(vkCreateWin32SurfaceKHR(vkInstance, &vkWin32SurfCreateInfo, nullptr, &vkSurface));
#endif

    VK_ASSERT(vkSurface != VK_NULL_HANDLE);

    VK_LOG_INFO("VkSurface initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkSurface;
}


static VkPhysicalDevice InitVkPhysDevice(VkInstance vkInstance)
{
    Timer timer;

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

    VK_LOG_INFO("VkPhysicalDevice initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return pickedPhysDevice;
}


static VkDevice InitVkDevice(VkPhysicalDevice vkPhysDevice, VkSurfaceKHR vkSurface, uint32_t& queueFamilyIndex, VkQueue& vkQueue)
{
    Timer timer;

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

    queueFamilyIndex = graphicsQueueFamilyIndex;

    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    
    const float queuePriority = 1.f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

    constexpr std::array deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VK_ASSERT(CheckVkDeviceExtensionsSupport(vkPhysDevice, deviceExtensions));

    deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &features13;

    deviceCreateInfo.pNext = &features2;

    VkDevice device = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(vkPhysDevice, &deviceCreateInfo, nullptr, &device));
    VK_ASSERT(device);
    
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &vkQueue);
    VK_ASSERT(vkQueue);

    VK_LOG_INFO("VkDevice initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return device;
}


static bool CheckVkSurfaceFormatSupport(VkPhysicalDevice vkPhysDevice, VkSurfaceKHR vkSurface, VkSurfaceFormatKHR format)
{
    uint32_t surfaceFormatsCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysDevice, vkSurface, &surfaceFormatsCount, nullptr));
    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatsCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysDevice, vkSurface, &surfaceFormatsCount, surfaceFormats.data()));

    if (surfaceFormatsCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
        return true;
    }

    for (VkSurfaceFormatKHR fmt : surfaceFormats) {
        if (fmt.format == format.format && fmt.colorSpace == format.colorSpace) {
            return true;
        }
    }

    return false;
}


static bool CheckVkPresentModeSupport(VkPhysicalDevice vkPhysDevice, VkSurfaceKHR vkSurface, VkPresentModeKHR presentMode)
{
    uint32_t presentModesCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysDevice, vkSurface, &presentModesCount, nullptr));
    std::vector<VkPresentModeKHR> presentModes(presentModesCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysDevice, vkSurface, &presentModesCount, presentModes.data()));

    for (VkPresentModeKHR mode : presentModes) {
        if (mode == presentMode) {
            return true;
        }
    }

    return false;
}


static VkSwapchainKHR InitVkSwapchain(VkPhysicalDevice vkPhysDevice, VkDevice vkDevice, VkSurfaceKHR vkSurface, 
    VkExtent2D requiredExtent, VkSwapchainKHR oldSwapchain, VkExtent2D& swapchainExtent)
{
    Timer timer;

    VkSurfaceCapabilitiesKHR surfCapabilities = {};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysDevice, vkSurface, &surfCapabilities));

    VkExtent2D extent = {};
    if (surfCapabilities.currentExtent.width != UINT32_MAX || surfCapabilities.currentExtent.height != UINT32_MAX) {
        extent = surfCapabilities.currentExtent;
    } else {
        extent.width = std::clamp(requiredExtent.width, surfCapabilities.minImageExtent.width, surfCapabilities.maxImageExtent.width);
        extent.height = std::clamp(requiredExtent.height, surfCapabilities.minImageExtent.height, surfCapabilities.maxImageExtent.height);
    }

    if (extent.width == 0 || extent.height == 0) {
        return oldSwapchain;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};

    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.oldSwapchain = oldSwapchain;
    swapchainCreateInfo.surface = vkSurface;
    swapchainCreateInfo.imageArrayLayers = 1u;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Since we have one queue for graphics, compute and transfer
    swapchainCreateInfo.imageExtent = extent;

    s_swapchainSurfFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
    s_swapchainSurfFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VK_ASSERT_MSG(CheckVkSurfaceFormatSupport(vkPhysDevice, vkSurface, s_swapchainSurfFormat), "Unsupported swapchain surface format");

    swapchainCreateInfo.imageFormat = s_swapchainSurfFormat.format;
    swapchainCreateInfo.imageColorSpace = s_swapchainSurfFormat.colorSpace;

    swapchainCreateInfo.minImageCount = surfCapabilities.minImageCount + 1;
    if (surfCapabilities.maxImageCount != 0) {
        swapchainCreateInfo.minImageCount = std::min(swapchainCreateInfo.minImageCount, surfCapabilities.maxImageCount);
    }

    swapchainCreateInfo.preTransform = (surfCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) 
        ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : surfCapabilities.currentTransform;

    swapchainCreateInfo.presentMode = CheckVkPresentModeSupport(vkPhysDevice, vkSurface, VK_PRESENT_MODE_MAILBOX_KHR) 
        ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;
    VK_ASSERT_MSG(CheckVkPresentModeSupport(vkPhysDevice, vkSurface, swapchainCreateInfo.presentMode), "Unsupported swapchain present mode");

    swapchainCreateInfo.clipped = VK_TRUE;

    VK_ASSERT(swapchainCreateInfo.minImageCount >= surfCapabilities.minImageCount);
    if (surfCapabilities.maxImageCount != 0) {
        VK_ASSERT(swapchainCreateInfo.minImageCount <= surfCapabilities.maxImageCount);
    }
    VK_ASSERT((swapchainCreateInfo.compositeAlpha & surfCapabilities.supportedCompositeAlpha) == swapchainCreateInfo.compositeAlpha);
    VK_ASSERT((swapchainCreateInfo.preTransform & surfCapabilities.supportedTransforms) == swapchainCreateInfo.preTransform);
    VK_ASSERT((swapchainCreateInfo.imageUsage & surfCapabilities.supportedUsageFlags) == swapchainCreateInfo.imageUsage);

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSwapchainKHR(vkDevice, &swapchainCreateInfo, nullptr, &swapchain));
    VK_ASSERT(swapchain);

    if (oldSwapchain) {
        vkDestroySwapchainKHR(vkDevice, oldSwapchain, nullptr);
    }

    swapchainExtent = swapchainCreateInfo.imageExtent;

    VK_LOG_INFO("VkSwapchain initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return swapchain;
}


static void GetVkSwapchainImages(VkDevice vkDevice, VkSwapchainKHR vkSwapchain, std::vector<VkImage>& swapchainImages)
{
    Timer timer;

    uint32_t swapchainImagesCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &swapchainImagesCount, nullptr));
    swapchainImages.resize(swapchainImagesCount);
    VK_CHECK(vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &swapchainImagesCount, swapchainImages.data()));

    VK_LOG_INFO("Getting VkSwapchain Images finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void InitVkSwapchainImageView(VkDevice vkDevice, const std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews)
{
    if (swapchainImages.empty()) {
        return;
    }

    Timer timer;

    swapchainImageViews.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainImages.size(); ++i) {
        VkImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = swapchainImages[i];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = s_swapchainSurfFormat.format;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        imageViewCreateInfo.subresourceRange.levelCount = 1;

        VkImageView& view = swapchainImageViews[i];

        VK_CHECK(vkCreateImageView(vkDevice, &imageViewCreateInfo, nullptr, &view));
    }

    VK_LOG_INFO("VkSwapchain Image Views initializing finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void DestroyVkSwapchainImageViews(VkDevice vkDevice, std::vector<VkImageView>& swapchainImageViews)
{
    Timer timer;

    if (swapchainImageViews.empty()) {
        return;
    }

    for (VkImageView& view : swapchainImageViews) {
        vkDestroyImageView(vkDevice, view, nullptr);
        view = VK_NULL_HANDLE;
    }

    swapchainImageViews.clear();

    VK_LOG_INFO("VkSwapchain Image Views destroying finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static VkSwapchainKHR RecreateVkSwapchain(VkPhysicalDevice vkPhysDevice, VkDevice vkDevice, VkSurfaceKHR vkSurface, VkExtent2D requiredExtent, 
    VkSwapchainKHR oldSwapchain, std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews, VkExtent2D& swapchainExtent)
{
    VK_CHECK(vkDeviceWaitIdle(vkDevice));

    VkSwapchainKHR vkSwapchain = InitVkSwapchain(vkPhysDevice, vkDevice, vkSurface, requiredExtent, oldSwapchain, swapchainExtent);
                
    if (vkSwapchain != VK_NULL_HANDLE && vkSwapchain != oldSwapchain) {
        DestroyVkSwapchainImageViews(vkDevice, swapchainImageViews);

        GetVkSwapchainImages(vkDevice, vkSwapchain, swapchainImages);
        InitVkSwapchainImageView(vkDevice, swapchainImages, swapchainImageViews);
    }

    return vkSwapchain;
}


VkCommandPool InitVkCmdPool(VkDevice vkDevice, uint32_t queueFamilyIndex)
{
    Timer timer;

    VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
    cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

    VkCommandPool cmdPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(vkDevice, &cmdPoolCreateInfo, nullptr, &cmdPool));
    VK_ASSERT(cmdPool != VK_NULL_HANDLE);

    VK_LOG_INFO("VkCommandPool initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return cmdPool;
}


static VkCommandBuffer AllocateVkCmdBuffer(VkDevice vkDevice, VkCommandPool vkCmdPool)
{
    Timer timer;

    VkCommandBufferAllocateInfo cmdBufferAllocInfo = {};
    cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufferAllocInfo.commandPool = vkCmdPool;
    cmdBufferAllocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(vkDevice, &cmdBufferAllocInfo, &cmdBuffer));
    VK_ASSERT(cmdBuffer != VK_NULL_HANDLE);

    VK_LOG_INFO("VkCommandBuffer allocating finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return cmdBuffer;
}


static VkShaderModule CreateVkShaderModule(VkDevice vkDevice, const fs::path& shaderSpirVPath, std::vector<uint8_t>* pExternalBuffer = nullptr)
{
    Timer timer;

    std::vector<uint8_t>* pShaderData = nullptr;
    std::vector<uint8_t> localBuffer;
    
    pShaderData = pExternalBuffer ? pExternalBuffer : &localBuffer;

    if (!ReadFile(*pShaderData, shaderSpirVPath)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", shaderSpirVPath.string().c_str());
    }
    VK_ASSERT_MSG(pShaderData->size() % sizeof(uint32_t) == 0, "Size of SPIR-V byte code of %s must be multiple of %zu", 
        shaderSpirVPath.string().c_str(), sizeof(uint32_t));

    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(pShaderData->data());
    shaderModuleCreateInfo.codeSize = pShaderData->size();

    VkShaderModule vkShaderModule = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(vkDevice, &shaderModuleCreateInfo, nullptr, &vkShaderModule));
    VK_ASSERT(vkShaderModule != VK_NULL_HANDLE);

    VK_LOG_INFO("Shader module \"%s\" creating finished: %f ms", shaderSpirVPath.string().c_str(), timer.End().GetDuration<float, std::milli>());

    return vkShaderModule;
}


static VkPipelineLayout InitVkPipelineLayout(VkDevice vkDevice)
{
    Timer timer;

    VkPipelineLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(vkDevice, &createInfo, nullptr, &layout));
    VK_ASSERT(layout != VK_NULL_HANDLE);

    VK_LOG_INFO("VkPipelineLayout initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return layout;
}


static VkPipeline InitVkGraphicsPipeline(VkDevice vkDevice, const fs::path& vsPath, const fs::path& psPath, VkPipelineLayout& layout)
{
    Timer timer;

    static constexpr size_t SHADER_STAGES_COUNT = 2;

    std::vector<uint8_t> shaderCodeBuffer;
    std::array<VkShaderModule, SHADER_STAGES_COUNT> vkShaderModules = {
        CreateVkShaderModule(vkDevice, vsPath, &shaderCodeBuffer),
        CreateVkShaderModule(vkDevice, psPath, &shaderCodeBuffer),
    };

    constexpr std::array<VkShaderStageFlagBits, SHADER_STAGES_COUNT> vkShaderStageBits = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };

    std::array<VkPipelineShaderStageCreateInfo, SHADER_STAGES_COUNT> shaderStageCreateInfos = {};
    for (size_t i = 0; i < shaderStageCreateInfos.size(); ++i) {
        VkPipelineShaderStageCreateInfo& info = shaderStageCreateInfos[i];

        info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.stage = vkShaderStageBits[i];
        info.module = vkShaderModules[i];
        info.pName = "main";
    }

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    
    pipelineCreateInfo.pStages = shaderStageCreateInfos.data();
    pipelineCreateInfo.stageCount = shaderStageCreateInfos.size();

    VkPipelineVertexInputStateCreateInfo vertexInputState = {};
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipelineCreateInfo.pVertexInputState = &vertexInputState;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;

    VkPipelineRasterizationStateCreateInfo rasterizationState = {};
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.lineWidth = 1.f;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;

    VkPipelineMultisampleStateCreateInfo multisampleState = {};
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.sampleShadingEnable = VK_FALSE;
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleState.minSampleShading = 1.f;
    multisampleState.pSampleMask = nullptr;
    multisampleState.alphaToCoverageEnable = VK_FALSE;
    multisampleState.alphaToOneEnable = VK_FALSE;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    
    VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.depthTestEnable = VK_FALSE;
    depthStencilState.depthWriteEnable = VK_FALSE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.front = {};
    depthStencilState.back = {};
    depthStencilState.minDepthBounds = 0.f;
    depthStencilState.maxDepthBounds = 1.f;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    pipelineCreateInfo.pViewportState = &viewportState;

    constexpr std::array dynStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = dynStates.size();
    dynamicState.pDynamicStates = dynStates.data();
    pipelineCreateInfo.pDynamicState = &dynamicState;

    // constexpr VkFormat colorAttachmentFormat = VK_FORMAT_B8G8R8A8_UNORM;
    
    VkPipelineRenderingCreateInfo dynRenderingCreateInfo = {};
    dynRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    // dynRenderingCreateInfo.colorAttachmentCount = 1;
    // dynRenderingCreateInfo.pColorAttachmentFormats = &colorAttachmentFormat;
    pipelineCreateInfo.pNext = &dynRenderingCreateInfo;

    layout = InitVkPipelineLayout(vkDevice);
    pipelineCreateInfo.layout = layout;

    VkPipeline vkPipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &vkPipeline));
    VK_ASSERT(vkPipeline != VK_NULL_HANDLE);

    for (VkShaderModule& shader : vkShaderModules) {
        vkDestroyShaderModule(vkDevice, shader, nullptr);
        shader = VK_NULL_HANDLE;
    }

    VK_LOG_INFO("VkPipeline (graphics) initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkPipeline;
}


int main(int argc, char* argv[])
{
    Timer timer;
    timer.Start();

    wndSysInit();
    BaseWindow* pWnd = wndSysGetMainWindow();

    WindowInitInfo wndInitInfo = {};
    wndInitInfo.title = "Vulkan Demo";
    wndInitInfo.width = 980;
    wndInitInfo.height = 640;

    pWnd->Init(wndInitInfo);
    ENG_ASSERT(pWnd->IsInitialized());

    VkDebugUtilsMessengerEXT vkDbgUtilsMessenger = VK_NULL_HANDLE;

    VkInstance vkInstance = InitVkInstance(wndInitInfo.title.data(), vkDbgUtilsMessenger);
    VkSurfaceKHR vkSurface = InitVkSurface(vkInstance, *pWnd);
    VkPhysicalDevice vkPhysDevice = InitVkPhysDevice(vkInstance);

    uint32_t queueFamilyIndex = UINT32_MAX;
    VkQueue vkQueue = VK_NULL_HANDLE;
    VkDevice vkDevice = InitVkDevice(vkPhysDevice, vkSurface, queueFamilyIndex, vkQueue);

    VkSwapchainKHR vkSwapchain = VK_NULL_HANDLE; // Assumed that OS will send resize event and swap chain will be created there
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkExtent2D swapchainExtent;

    VkCommandPool vkCmdPool = InitVkCmdPool(vkDevice, queueFamilyIndex);
    VkCommandBuffer vkCmdBuffer = AllocateVkCmdBuffer(vkDevice, vkCmdPool);

    VkPipelineLayout vkPipelineLayout = VK_NULL_HANDLE;
    VkPipeline vkPipeline = InitVkGraphicsPipeline(vkDevice, "shaders/bin/test.vert.spv", "shaders/bin/test.frag.spv", vkPipelineLayout);

    while(!pWnd->IsClosed()) {
        pWnd->ProcessEvents();
        
        WndEvent event;
        while(pWnd->PopEvent(event)) {
            if (event.Is<WndResizeEvent>()) {
                // Also when VK_ERROR_OUT_OF_DATE_KHR / VK_SUBOPTIMAL_KHR
                const VkSwapchainKHR oldSwapchain = vkSwapchain;
                const VkExtent2D requiredExtent = { pWnd->GetWidth(), pWnd->GetHeight() };
                
                vkSwapchain = RecreateVkSwapchain(vkPhysDevice, vkDevice, vkSurface, requiredExtent, oldSwapchain, 
                    swapchainImages, swapchainImageViews, swapchainExtent);
            }
        }

        if (vkSwapchain == VK_NULL_HANDLE) {
            continue;
        }
    }

    VK_CHECK(vkDeviceWaitIdle(vkDevice));

    vkDestroyPipelineLayout(vkDevice, vkPipelineLayout, nullptr);
    vkDestroyPipeline(vkDevice, vkPipeline, nullptr);

    vkDestroyCommandPool(vkDevice, vkCmdPool, nullptr);

    DestroyVkSwapchainImageViews(vkDevice, swapchainImageViews);

    vkDestroySwapchainKHR(vkDevice, vkSwapchain, nullptr);

    vkDestroyDevice(vkDevice, nullptr);
    vkDestroySurfaceKHR(vkInstance, vkSurface, nullptr);
    
    DestroyVkDebugMessenger(vkInstance, vkDbgUtilsMessenger);
    vkDestroyInstance(vkInstance, nullptr);

    pWnd->Destroy();
    wndSysTerminate();

    return 0;
}