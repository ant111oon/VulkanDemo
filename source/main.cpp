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

    VkApplicationInfo vkApplicationInfo = {};
    vkApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    vkApplicationInfo.pApplicationName = wndInitInfo.title.data();
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
#endif

#ifdef ENG_BUILD_DEBUG
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

    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
#ifdef ENG_OS_WINDOWS
    VkWin32SurfaceCreateInfoKHR vkWin32SurfCreateInfo = {};
    vkWin32SurfCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    vkWin32SurfCreateInfo.hinstance = GetModuleHandle(nullptr);
    vkWin32SurfCreateInfo.hwnd = (HWND)pWnd->GetNativeHandle();

    VK_CHECK(vkCreateWin32SurfaceKHR(vkInstance, &vkWin32SurfCreateInfo, nullptr, &vkSurface));
#endif
    VK_ASSERT(vkSurface != VK_NULL_HANDLE);

    while(!pWnd->IsClosed()) {
        pWnd->ProcessEvents();
        
        WndEvent event;
        while(pWnd->PopEvent(event)) {
        }
    }

    vkDestroySurfaceKHR(vkInstance, vkSurface, nullptr);
    
    // NOTE: Debug messenger is automatically created and destroyed by the Vulkan loader via pNext.
    vkDestroyInstance(vkInstance, nullptr);

    pWnd->Destroy();
    wndSysTerminate();

    return 0;
}