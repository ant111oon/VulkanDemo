#include "core/engine/wnd_system/wnd_system.h"

#include "core/platform/file/file.h"
#include "core/utils/timer.h"

#include "render/core/vulkan/vk_instance.h"
#include "render/core/vulkan/vk_surface.h"
#include "render/core/vulkan/vk_phys_device.h"
#include "render/core/vulkan/vk_device.h"
#include "render/core/vulkan/vk_swapchain.h"
#include "render/core/vulkan/vk_fence.h"
#include "render/core/vulkan/vk_semaphore.h"
#include "render/core/vulkan/vk_cmd.h"
#include "render/core/vulkan/vk_buffer.h"
#include "render/core/vulkan/vk_image.h"
#include "render/core/vulkan/vk_pipeline.h"
#include "render/core/vulkan/vk_query.h"

#include "core/engine/camera/camera.h"

#include "core/profiler/cpu_profiler.h"
#include "render/core/vulkan/vk_profiler.h"


#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#include <tiny_gltf.h>


namespace fs = std::filesystem;
namespace gltf = tinygltf;

using VertexIndexType = uint32_t;


struct Mesh
{
    uint32_t firstVertex;
    uint32_t vertexCount;
    uint32_t firstIndex;
    uint32_t indexCount;
};


struct Vertex
{
    glm::uint posXY;
    glm::uint posZnormX;
    glm::uint normYZ;
    glm::uint texcoord;
};


struct TEST_BINDLESS_REGISTRY
{
    VkDeviceAddress VERTEX_DATA;

    glm::uint RENDER_INFO_IDX;
    glm::uint PADDING;
};


struct COMMON_RENDER_INFO
{
    glm::uint MESH_IDX;
    glm::uint MATERIAL_IDX;
    glm::uint TRANSFORM_IDX;
    glm::uint PADDING;
};


struct COMMON_MATERIAL
{
    int ALBEDO_TEX_IDX = -1;
    int NORMAL_TEX_IDX = -1;
    int MR_TEX_IDX = -1;
    int AO_TEX_IDX = -1;
    int EMISSIVE_TEX_IDX = -1;
};


struct COMMON_TRANSFORM
{
    glm::vec4 MATR[3];
};


struct COMMON_CB_DATA
{
    glm::mat4x4 COMMON_VIEW_MATRIX;
    glm::mat4x4 COMMON_PROJ_MATRIX;
    glm::mat4x4 COMMON_VIEW_PROJ_MATRIX;

    glm::uint  COMMON_FLAGS;
    glm::uint  COMMON_DBG_FLAGS;
    glm::uvec2 PADDING;
};


enum class COMMON_DBG_FLAG_MASKS
{
    OUTPUT_COMMON_MTL_ALBEDO_TEX = 0x1,
    OUTPUT_COMMON_MTL_NORMAL_TEX = 0x2,
    OUTPUT_COMMON_MTL_MR_TEX = 0x4,
    OUTPUT_COMMON_MTL_AO_TEX = 0x8,
    OUTPUT_COMMON_MTL_EMISSIVE_TEX = 0x10,
};


enum class COMMON_SAMPLER_IDX : glm::uint
{
    NEAREST_REPEAT,
    NEAREST_MIRRORED_REPEAT,
    NEAREST_CLAMP_TO_EDGE,
    NEAREST_CLAMP_TO_BORDER,
    NEAREST_MIRROR_CLAMP_TO_EDGE,

    LINEAR_REPEAT,
    LINEAR_MIRRORED_REPEAT,
    LINEAR_CLAMP_TO_EDGE,
    LINEAR_CLAMP_TO_BORDER,
    LINEAR_MIRROR_CLAMP_TO_EDGE,

    ANISO_2X_NEAREST_REPEAT,
    ANISO_2X_NEAREST_MIRRORED_REPEAT,
    ANISO_2X_NEAREST_CLAMP_TO_EDGE,
    ANISO_2X_NEAREST_CLAMP_TO_BORDER,
    ANISO_2X_NEAREST_MIRROR_CLAMP_TO_EDGE,

    ANISO_2X_LINEAR_REPEAT,
    ANISO_2X_LINEAR_MIRRORED_REPEAT,
    ANISO_2X_LINEAR_CLAMP_TO_EDGE,
    ANISO_2X_LINEAR_CLAMP_TO_BORDER,
    ANISO_2X_LINEAR_MIRROR_CLAMP_TO_EDGE,

    ANISO_4X_NEAREST_REPEAT,
    ANISO_4X_NEAREST_MIRRORED_REPEAT,
    ANISO_4X_NEAREST_CLAMP_TO_EDGE,
    ANISO_4X_NEAREST_CLAMP_TO_BORDER,
    ANISO_4X_NEAREST_MIRROR_CLAMP_TO_EDGE,

    ANISO_4X_LINEAR_REPEAT,
    ANISO_4X_LINEAR_MIRRORED_REPEAT,
    ANISO_4X_LINEAR_CLAMP_TO_EDGE,
    ANISO_4X_LINEAR_CLAMP_TO_BORDER,
    ANISO_4X_LINEAR_MIRROR_CLAMP_TO_EDGE,

    ANISO_8X_NEAREST_REPEAT,
    ANISO_8X_NEAREST_MIRRORED_REPEAT,
    ANISO_8X_NEAREST_CLAMP_TO_EDGE,
    ANISO_8X_NEAREST_CLAMP_TO_BORDER,
    ANISO_8X_NEAREST_MIRROR_CLAMP_TO_EDGE,

    ANISO_8X_LINEAR_REPEAT,
    ANISO_8X_LINEAR_MIRRORED_REPEAT,
    ANISO_8X_LINEAR_CLAMP_TO_EDGE,
    ANISO_8X_LINEAR_CLAMP_TO_BORDER,
    ANISO_8X_LINEAR_MIRROR_CLAMP_TO_EDGE,

    ANISO_16X_NEAREST_REPEAT,
    ANISO_16X_NEAREST_MIRRORED_REPEAT,
    ANISO_16X_NEAREST_CLAMP_TO_EDGE,
    ANISO_16X_NEAREST_CLAMP_TO_BORDER,
    ANISO_16X_NEAREST_MIRROR_CLAMP_TO_EDGE,

    ANISO_16X_LINEAR_REPEAT,
    ANISO_16X_LINEAR_MIRRORED_REPEAT,
    ANISO_16X_LINEAR_CLAMP_TO_EDGE,
    ANISO_16X_LINEAR_CLAMP_TO_BORDER,
    ANISO_16X_LINEAR_MIRROR_CLAMP_TO_EDGE,

    COUNT
};


static constexpr uint32_t COMMON_MTL_TEXTURES_COUNT = 128;


static constexpr size_t MAX_VERTEX_COUNT = 512 * 1024;
static constexpr size_t VERTEX_BUFFER_SIZE_BYTES = MAX_VERTEX_COUNT * sizeof(Vertex);

static constexpr size_t MAX_INDEX_COUNT = 2'000'000;
static constexpr size_t INDEX_BUFFER_SIZE_BYTES = MAX_INDEX_COUNT * sizeof(VertexIndexType);

static constexpr const char* APP_NAME = "Vulkan Demo";

static constexpr bool VSYNC_ENABLED = false;

static constexpr float CAMERA_SPEED = 0.05f;

static constexpr const char* DBG_TEX_OUTPUT_NAMES[] = {
    "ALBEDO",
    "NORMAL",
    "MR",
    "AO",
    "EMISSIVE"
};

static constexpr const char* COMMON_SAMPLERS_DBG_NAMES[] = {
    "NEAREST_REPEAT",
    "NEAREST_MIRRORED_REPEAT",
    "NEAREST_CLAMP_TO_EDGE",
    "NEAREST_CLAMP_TO_BORDER",
    "NEAREST_MIRROR_CLAMP_TO_EDGE",

    "LINEAR_REPEAT",
    "LINEAR_MIRRORED_REPEAT",
    "LINEAR_CLAMP_TO_EDGE",
    "LINEAR_CLAMP_TO_BORDER",
    "LINEAR_MIRROR_CLAMP_TO_EDGE",

    "ANISO_2X_NEAREST_REPEAT",
    "ANISO_2X_NEAREST_MIRRORED_REPEAT",
    "ANISO_2X_NEAREST_CLAMP_TO_EDGE",
    "ANISO_2X_NEAREST_CLAMP_TO_BORDER",
    "ANISO_2X_NEAREST_MIRROR_CLAMP_TO_EDGE",

    "ANISO_2X_LINEAR_REPEAT",
    "ANISO_2X_LINEAR_MIRRORED_REPEAT",
    "ANISO_2X_LINEAR_CLAMP_TO_EDGE",
    "ANISO_2X_LINEAR_CLAMP_TO_BORDER",
    "ANISO_2X_LINEAR_MIRROR_CLAMP_TO_EDGE",

    "ANISO_4X_NEAREST_REPEAT",
    "ANISO_4X_NEAREST_MIRRORED_REPEAT",
    "ANISO_4X_NEAREST_CLAMP_TO_EDGE",
    "ANISO_4X_NEAREST_CLAMP_TO_BORDER",
    "ANISO_4X_NEAREST_MIRROR_CLAMP_TO_EDGE",

    "ANISO_4X_LINEAR_REPEAT",
    "ANISO_4X_LINEAR_MIRRORED_REPEAT",
    "ANISO_4X_LINEAR_CLAMP_TO_EDGE",
    "ANISO_4X_LINEAR_CLAMP_TO_BORDER",
    "ANISO_4X_LINEAR_MIRROR_CLAMP_TO_EDGE",

    "ANISO_8X_NEAREST_REPEAT",
    "ANISO_8X_NEAREST_MIRRORED_REPEAT",
    "ANISO_8X_NEAREST_CLAMP_TO_EDGE",
    "ANISO_8X_NEAREST_CLAMP_TO_BORDER",
    "ANISO_8X_NEAREST_MIRROR_CLAMP_TO_EDGE",

    "ANISO_8X_LINEAR_REPEAT",
    "ANISO_8X_LINEAR_MIRRORED_REPEAT",
    "ANISO_8X_LINEAR_CLAMP_TO_EDGE",
    "ANISO_8X_LINEAR_CLAMP_TO_BORDER",
    "ANISO_8X_LINEAR_MIRROR_CLAMP_TO_EDGE",

    "ANISO_16X_NEAREST_REPEAT",
    "ANISO_16X_NEAREST_MIRRORED_REPEAT",
    "ANISO_16X_NEAREST_CLAMP_TO_EDGE",
    "ANISO_16X_NEAREST_CLAMP_TO_BORDER",
    "ANISO_16X_NEAREST_MIRROR_CLAMP_TO_EDGE",

    "ANISO_16X_LINEAR_REPEAT",
    "ANISO_16X_LINEAR_MIRRORED_REPEAT",
    "ANISO_16X_LINEAR_CLAMP_TO_EDGE",
    "ANISO_16X_LINEAR_CLAMP_TO_BORDER",
    "ANISO_16X_LINEAR_MIRROR_CLAMP_TO_EDGE",
};

static Window* s_pWnd = nullptr;

static vkn::Instance& s_vkInstance = vkn::GetInstance();
static vkn::Surface& s_vkSurface = vkn::GetSurface();

static vkn::PhysicalDevice& s_vkPhysDevice = vkn::GetPhysicalDevice();

static vkn::Device& s_vkDevice = vkn::GetDevice();

static vkn::Swapchain& s_vkSwapchain = vkn::GetSwapchain();

static vkn::CmdPool   s_vkCmdPool;
static vkn::CmdBuffer s_vkImmediateSubmitCmdBuffer;

static VkDescriptorPool      s_vkDescriptorPool = VK_NULL_HANDLE;
static VkDescriptorSet       s_vkDescriptorSet = VK_NULL_HANDLE;
static VkDescriptorSetLayout s_vkDescriptorSetLayout = VK_NULL_HANDLE;

static VkPipelineLayout      s_vkPipelineLayout = VK_NULL_HANDLE;
static VkPipeline            s_vkPipeline = VK_NULL_HANDLE;

static std::vector<vkn::Semaphore> s_vkRenderingFinishedSemaphores;
static vkn::Semaphore s_vkPresentFinishedSemaphore;
static vkn::Fence     s_vkRenderingFinishedFence;
static vkn::CmdBuffer s_vkRenderCmdBuffer;

static vkn::Fence s_vkImmediateSubmitFinishedFence;

static vkn::Image s_vkDepthImage;
static vkn::ImageView s_vkDepthImageView;

static vkn::Buffer s_vertexBuffer;
static vkn::Buffer s_indexBuffer;

static vkn::Buffer s_commonConstBuffer;

static vkn::Buffer s_commonRenderInfoBuffer;
static vkn::Buffer s_commonMaterialsBuffer;
static vkn::Buffer s_commonTransformsBuffer;

static vkn::QueryPool s_vkQueryPool;

static std::vector<COMMON_RENDER_INFO>  s_sceneRenderInfos;
static std::vector<COMMON_MATERIAL>     s_sceneMaterials;
static std::vector<Mesh>                s_sceneMeshes;
static std::vector<COMMON_TRANSFORM>    s_sceneTransforms;

static std::vector<vkn::Image>     s_sceneImages;
static std::vector<vkn::ImageView> s_sceneImageViews;
static std::vector<vkn::Sampler>   s_commonSamplers;

static vkn::Image     s_sceneDefaultImage;
static vkn::ImageView s_sceneDefaultImageView;

static eng::Camera s_camera;

static uint32_t s_dbgTexIdx = 0;

static size_t s_frameNumber = 0;
static bool s_swapchainRecreateRequired = false;
static bool s_flyCameraMode = false;


namespace tinygltf
{
    static constexpr VkFormat GetImageVkFormatR(uint32_t pixelType, bool isSRGB)
    {
        if (isSRGB) {
            CORE_ASSERT_MSG(pixelType == TINYGLTF_COMPONENT_TYPE_BYTE || pixelType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE,
                "If texture is in sRGB, it must be 8-bit per component");
        }
    
        switch (pixelType) {
            case TINYGLTF_COMPONENT_TYPE_BYTE:           return isSRGB ? VK_FORMAT_R8_SRGB : VK_FORMAT_R8_SNORM;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  return isSRGB ? VK_FORMAT_R8_SRGB : VK_FORMAT_R8_UNORM;
            case TINYGLTF_COMPONENT_TYPE_SHORT:          return VK_FORMAT_R16_SNORM;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return VK_FORMAT_R16_UNORM;
            case TINYGLTF_COMPONENT_TYPE_INT:            return VK_FORMAT_R32_SINT;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   return VK_FORMAT_R32_UINT;
            case TINYGLTF_COMPONENT_TYPE_FLOAT:          return VK_FORMAT_R32_SFLOAT;
            case TINYGLTF_COMPONENT_TYPE_DOUBLE:         return VK_FORMAT_R64_SFLOAT;
        }
    
        CORE_ASSERT_FAIL("Unsupported R image format combitaion. pixel_type = %u", pixelType);
        return VK_FORMAT_UNDEFINED;
    }
    
    
    static constexpr VkFormat  GetImageVkFormatRG(uint32_t pixelType, bool isSRGB)
    {
        if (isSRGB) {
            CORE_ASSERT_MSG(pixelType == TINYGLTF_COMPONENT_TYPE_BYTE || pixelType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE,
                "If texture is in sRGB, it must be 8-bit per component");
        }
    
        switch (pixelType) {
            case TINYGLTF_COMPONENT_TYPE_BYTE:           return isSRGB ? VK_FORMAT_R8G8_SRGB : VK_FORMAT_R8G8_SNORM;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  return isSRGB ? VK_FORMAT_R8G8_SRGB : VK_FORMAT_R8G8_UNORM;
            case TINYGLTF_COMPONENT_TYPE_SHORT:          return VK_FORMAT_R16G16_SNORM;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return VK_FORMAT_R16G16_UNORM;
            case TINYGLTF_COMPONENT_TYPE_INT:            return VK_FORMAT_R32G32_SINT;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   return VK_FORMAT_R32G32_UINT;
            case TINYGLTF_COMPONENT_TYPE_FLOAT:          return VK_FORMAT_R32G32_SFLOAT;
            case TINYGLTF_COMPONENT_TYPE_DOUBLE:         return VK_FORMAT_R64G64_SFLOAT;
        }
    
        CORE_ASSERT_FAIL("Unsupported RG image format combitaion. pixel_type = %u", pixelType);
        return VK_FORMAT_UNDEFINED;
    }
    
    
    static constexpr VkFormat GetImageVkFormatRGB(uint32_t pixelType, bool isSRGB)
    {
        if (isSRGB) {
            CORE_ASSERT_MSG(pixelType == TINYGLTF_COMPONENT_TYPE_BYTE || pixelType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE,
                "If texture is in sRGB, it must be 8-bit per component");
        }
    
        switch (pixelType) {
            case TINYGLTF_COMPONENT_TYPE_BYTE:           return isSRGB ? VK_FORMAT_R8G8B8_SRGB : VK_FORMAT_R8G8B8_SNORM;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  return isSRGB ? VK_FORMAT_R8G8B8_SRGB : VK_FORMAT_R8G8B8_UNORM;
            case TINYGLTF_COMPONENT_TYPE_SHORT:          return VK_FORMAT_R16G16B16_SNORM;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return VK_FORMAT_R16G16B16_UNORM;
            case TINYGLTF_COMPONENT_TYPE_INT:            return VK_FORMAT_R32G32B32_SINT;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   return VK_FORMAT_R32G32B32_UINT;
            case TINYGLTF_COMPONENT_TYPE_FLOAT:          return VK_FORMAT_R32G32B32_SFLOAT;
            case TINYGLTF_COMPONENT_TYPE_DOUBLE:         return VK_FORMAT_R64G64B64_SFLOAT;
        }
    
        CORE_ASSERT_FAIL("Unsupported RGB image format combitaion. pixel_type = %u", pixelType);
        return VK_FORMAT_UNDEFINED;
    }
    
    
    static constexpr VkFormat GetImageVkFormatRGBA(uint32_t pixelType, bool isSRGB)
    {
        if (isSRGB) {
            CORE_ASSERT_MSG(pixelType == TINYGLTF_COMPONENT_TYPE_BYTE || pixelType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE,
                "If texture is in sRGB, it must be 8-bit per component");
        }
    
        switch (pixelType) {
            case TINYGLTF_COMPONENT_TYPE_BYTE:           return isSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_SNORM;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  return isSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
            case TINYGLTF_COMPONENT_TYPE_SHORT:          return VK_FORMAT_R16G16B16A16_SNORM;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return VK_FORMAT_R16G16B16A16_UNORM;
            case TINYGLTF_COMPONENT_TYPE_INT:            return VK_FORMAT_R32G32B32A32_SINT;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   return VK_FORMAT_R32G32B32A32_UINT;
            case TINYGLTF_COMPONENT_TYPE_FLOAT:          return VK_FORMAT_R32G32B32A32_SFLOAT;
            case TINYGLTF_COMPONENT_TYPE_DOUBLE:         return VK_FORMAT_R64G64B64A64_SFLOAT;
        }
    
        CORE_ASSERT_FAIL("Unsupported RGBA image format combitaion. pixel_type = %u", pixelType);
        return VK_FORMAT_UNDEFINED;
    }
    
    
    static constexpr VkFormat GetImageVkFormat(uint32_t component, uint32_t pixelType, bool isSRGB)
    {
        switch (component) {
            case 1: return GetImageVkFormatR(pixelType, isSRGB);
            case 2: return GetImageVkFormatRG(pixelType, isSRGB);
            case 3: return GetImageVkFormatRGB(pixelType, isSRGB);
            case 4: return GetImageVkFormatRGBA(pixelType, isSRGB);
        }
    
        CORE_ASSERT_FAIL("Unsupported image format combitaion. pixel_type = %u, component = %u", pixelType, component);
        return VK_FORMAT_UNDEFINED;
    }
}


#ifdef ENG_VK_DEBUG_UTILS_ENABLED
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


static constexpr VkIndexType GetVkIndexType()
{
    static_assert(std::is_same_v<VertexIndexType, uint8_t> || std::is_same_v<VertexIndexType, uint16_t> || std::is_same_v<VertexIndexType, uint32_t>);

    if constexpr (std::is_same_v<VertexIndexType, uint8_t>) {
        return VK_INDEX_TYPE_UINT8;
    } else if constexpr (std::is_same_v<VertexIndexType, uint16_t>) {
        return VK_INDEX_TYPE_UINT16;
    } else {
        return VK_INDEX_TYPE_UINT32;
    }
}


static void CreateVkInstance()
{
#ifdef ENG_VK_DEBUG_UTILS_ENABLED
    vkn::InstanceDebugMessengerCreateInfo vkDbgMessengerCreateInfo = {};
    vkDbgMessengerCreateInfo.messageType = 
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    vkDbgMessengerCreateInfo.messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    vkDbgMessengerCreateInfo.pMessageCallback = DbgVkMessageCallback;

    constexpr std::array vkInstLayers = {
        "VK_LAYER_KHRONOS_validation",
    };
#endif

    constexpr std::array vkInstExtensions = {
    #ifdef ENG_VK_DEBUG_UTILS_ENABLED
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    #endif

        VK_KHR_SURFACE_EXTENSION_NAME,
    #ifdef ENG_OS_WINDOWS
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    #endif
    };

    vkn::InstanceCreateInfo vkInstCreateInfo = {};
    vkInstCreateInfo.pApplicationName = APP_NAME;
    vkInstCreateInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    vkInstCreateInfo.pEngineName = "VkEngine";
    vkInstCreateInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    vkInstCreateInfo.apiVersion = VK_API_VERSION_1_3;
    vkInstCreateInfo.extensions = vkInstExtensions;
#ifdef ENG_VK_DEBUG_UTILS_ENABLED
    vkInstCreateInfo.layers = vkInstLayers;
    vkInstCreateInfo.pDbgMessengerCreateInfo = &vkDbgMessengerCreateInfo;
#endif

    s_vkInstance.Create(vkInstCreateInfo); 
    CORE_ASSERT(s_vkInstance.IsCreated());
}


static void CreateVkSwapchain()
{
    vkn::SwapchainCreateInfo vkSwapchainCreateInfo = {};
    vkSwapchainCreateInfo.pDevice = &s_vkDevice;
    vkSwapchainCreateInfo.pSurface = &s_vkSurface;

    vkSwapchainCreateInfo.width = s_pWnd->GetWidth();
    vkSwapchainCreateInfo.height = s_pWnd->GetHeight();

    vkSwapchainCreateInfo.minImageCount    = 2;
    vkSwapchainCreateInfo.imageFormat      = VK_FORMAT_B8G8R8A8_SRGB;
    vkSwapchainCreateInfo.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    vkSwapchainCreateInfo.imageArrayLayers = 1u;
    vkSwapchainCreateInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    vkSwapchainCreateInfo.transform        = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    vkSwapchainCreateInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    vkSwapchainCreateInfo.presentMode      = VSYNC_ENABLED ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

    s_vkSwapchain.Create(vkSwapchainCreateInfo);
    CORE_ASSERT(s_vkSwapchain.IsCreated());
}


static void CreateVkPhysAndLogicalDevices()
{
    vkn::PhysicalDeviceFeaturesRequirenments vkPhysDeviceFeturesReq = {};
    vkPhysDeviceFeturesReq.independentBlend = true;
    vkPhysDeviceFeturesReq.descriptorBindingPartiallyBound = true;
    vkPhysDeviceFeturesReq.runtimeDescriptorArray = true;
    vkPhysDeviceFeturesReq.samplerAnisotropy = true;
    vkPhysDeviceFeturesReq.samplerMirrorClampToEdge = true;

    vkn::PhysicalDevicePropertiesRequirenments vkPhysDevicePropsReq = {};
    vkPhysDevicePropsReq.deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

    vkn::PhysicalDeviceCreateInfo vkPhysDeviceCreateInfo = {};
    vkPhysDeviceCreateInfo.pInstance = &s_vkInstance;
    vkPhysDeviceCreateInfo.pPropertiesRequirenments = &vkPhysDevicePropsReq;
    vkPhysDeviceCreateInfo.pFeaturesRequirenments = &vkPhysDeviceFeturesReq;

    s_vkPhysDevice.Create(vkPhysDeviceCreateInfo);
    CORE_ASSERT(s_vkPhysDevice.IsCreated()); 


    constexpr std::array vkDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VK_ASSERT(s_vkPhysDevice.GetFeatures13().dynamicRendering);
    VK_ASSERT(s_vkPhysDevice.GetFeatures13().synchronization2);

    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    VK_ASSERT(s_vkPhysDevice.GetFeatures12().bufferDeviceAddress);
    VK_ASSERT(s_vkPhysDevice.GetFeatures12().descriptorBindingPartiallyBound);
    VK_ASSERT(s_vkPhysDevice.GetFeatures12().runtimeDescriptorArray);
    VK_ASSERT(s_vkPhysDevice.GetFeatures12().samplerMirrorClampToEdge);

    VkPhysicalDeviceVulkan12Features features12 = {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = &features13;
    features12.bufferDeviceAddress = VK_TRUE;
    features12.descriptorBindingPartiallyBound = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;
    features12.samplerMirrorClampToEdge = VK_TRUE;

    VK_ASSERT(s_vkPhysDevice.GetFeatures11().shaderDrawParameters);

    VkPhysicalDeviceVulkan11Features features11 = {};
    features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features11.pNext = &features12;
    features11.shaderDrawParameters = VK_TRUE; // Enables slang internal shader variables like "SV_VertexID" etc.

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &features11;
    features2.features.samplerAnisotropy = VK_TRUE;

    vkn::DeviceCreateInfo vkDeviceCreateInfo = {};
    vkDeviceCreateInfo.pPhysDevice = &s_vkPhysDevice;
    vkDeviceCreateInfo.pSurface = &s_vkSurface;
    vkDeviceCreateInfo.queuePriority = 1.f;
    vkDeviceCreateInfo.extensions = vkDeviceExtensions;
    vkDeviceCreateInfo.pFeatures2 = &features2;

    s_vkDevice.Create(vkDeviceCreateInfo);
    CORE_ASSERT(s_vkDevice.IsCreated());
}


static VkShaderModule CreateVkShaderModule(VkDevice vkDevice, const fs::path& shaderSpirVPath, std::vector<uint8_t>* pExternalBuffer = nullptr)
{
    Timer timer;

    std::vector<uint8_t>* pShaderData = nullptr;
    std::vector<uint8_t> localBuffer;
    
    pShaderData = pExternalBuffer ? pExternalBuffer : &localBuffer;

    const std::string pathS = shaderSpirVPath.string();

    if (!ReadFile(*pShaderData, shaderSpirVPath)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", pathS.c_str());
    }
    VK_ASSERT_MSG(pShaderData->size() % sizeof(uint32_t) == 0, "Size of SPIR-V byte code of %s must be multiple of %zu", pathS.c_str(), sizeof(uint32_t));

    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(pShaderData->data());
    shaderModuleCreateInfo.codeSize = pShaderData->size();

    VkShaderModule vkShaderModule = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(vkDevice, &shaderModuleCreateInfo, nullptr, &vkShaderModule));
    VK_ASSERT(vkShaderModule != VK_NULL_HANDLE);

    VK_LOG_INFO("Shader module \"%s\" creating finished: %f ms", pathS.c_str(), timer.End().GetDuration<float, std::milli>());

    return vkShaderModule;
}


static VkDescriptorPool CreateVkDescriptorPool(VkDevice vkDevice)
{
    Timer timer;

    vkn::DescriptorPoolBuilder builder;

    builder
        // .SetFlags(VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT)
        .SetMaxDescriptorSetsCount(1);
        
    builder
        .AddResource(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
        .AddResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3)
        .AddResource(VK_DESCRIPTOR_TYPE_SAMPLER, (uint32_t)COMMON_SAMPLER_IDX::COUNT)
        .AddResource(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, COMMON_MTL_TEXTURES_COUNT);
    
    VkDescriptorPool vkPool = builder.Build(vkDevice);

    CORE_LOG_INFO("VkDescriptorPool creating finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkPool;
}


static VkDescriptorSetLayout CreateVkDescriptorSetLayout(VkDevice vkDevice)
{
    Timer timer;

    vkn::DescriptorSetLayoutBuilder builder;

    builder
        // .SetFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT)
        .AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, (uint32_t)COMMON_SAMPLER_IDX::COUNT, VK_SHADER_STAGE_ALL)
        .AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL)
        .AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL)
        .AddBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL)
        .AddBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL)
        .AddBinding(5, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, COMMON_MTL_TEXTURES_COUNT, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout vkLayout = builder.Build(vkDevice);

    CORE_LOG_INFO("VkDescriptorSetLayout creating finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkLayout;
}


static VkDescriptorSet CreateVkDescriptorSet(VkDevice vkDevice, VkDescriptorPool vkDescriptorPool, VkDescriptorSetLayout vkDescriptorSetLayout)
{
    Timer timer;

    vkn::DescriptorSetAllocator allocator;

    VkDescriptorSet vkDescriptorSets[] = { VK_NULL_HANDLE };

    allocator
        .SetPool(vkDescriptorPool)
        .AddLayout(vkDescriptorSetLayout)
        .Allocate(vkDevice, vkDescriptorSets);

    CORE_LOG_INFO("VkDescriptorSet allocating finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkDescriptorSets[0];
}


static VkPipelineLayout CreateVkPipelineLayout(VkDevice vkDevice, VkDescriptorSetLayout vkDescriptorSetLayout)
{
    Timer timer;

    vkn::PipelineLayoutBuilder plBuilder(s_vkPhysDevice.GetProperties().limits.maxPushConstantsSize);

    VkPipelineLayout vkLayout = plBuilder
        .AddPushConstantRange(VK_SHADER_STAGE_ALL, 0, sizeof(TEST_BINDLESS_REGISTRY))
        .AddDescriptorSetLayout(vkDescriptorSetLayout)
        .Build(vkDevice);

    CORE_LOG_INFO("VkPipelineLayout initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkLayout;
}


static VkPipeline CreateVkGraphicsPipeline(VkDevice vkDevice, VkPipelineLayout vkLayout, const fs::path& vsPath, const fs::path& psPath)
{
    Timer timer;

    std::vector<uint8_t> shaderCodeBuffer;
    std::array vkShaderModules = {
        CreateVkShaderModule(vkDevice, vsPath, &shaderCodeBuffer),
        CreateVkShaderModule(vkDevice, psPath, &shaderCodeBuffer),
    };

    std::array vkShaderModuleStages = {
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    static_assert(vkShaderModules.size() == vkShaderModuleStages.size());
    const size_t shadersCount = vkShaderModules.size();

    VkPipelineColorBlendAttachmentState blendState = {};
    blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    vkn::GraphicsPipelineBuilder builder;

    for (size_t i = 0; i < shadersCount; ++i) {
        builder.AddShader(vkShaderModules[i], vkShaderModuleStages[i], "main");
    }
    
    VkPipeline vkPipeline = builder
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(1.f)
        .SetStencilTestState(VK_FALSE, {}, {})
    #ifdef ENG_REVERSED_Z
        .SetDepthTestState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER_OR_EQUAL)
    #else
        .SetDepthTestState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
    #endif
        .SetDepthBoundsTestState(VK_TRUE, 0.f, 1.f)
        .SetDepthAttachmentFormat(VK_FORMAT_D32_SFLOAT)
        .AddColorAttachmentFormat(s_vkSwapchain.GetImageFormat())
        .AddColorBlendAttachment(blendState)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .SetLayout(vkLayout)
        .Build(vkDevice);

    for (VkShaderModule& shader : vkShaderModules) {
        vkDestroyShaderModule(vkDevice, shader, nullptr);
        shader = VK_NULL_HANDLE;
    }

    CORE_LOG_INFO("VkPipeline (graphics) initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());

    return vkPipeline;
}


static void CreateCommonSamplers()
{
    ENG_PROFILE_SCOPED_MARKER_C("CreateCommonSamplers", 225, 0, 225, 255);

    Timer timer;

    s_commonSamplers.resize((uint32_t)COMMON_SAMPLER_IDX::COUNT);

    std::vector<vkn::SamplerCreateInfo> smpCreateInfos((uint32_t)COMMON_SAMPLER_IDX::COUNT);

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].pDevice = &s_vkDevice;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].magFilter = VK_FILTER_NEAREST;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].minFilter = VK_FILTER_NEAREST;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].mipLodBias = 0.f;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].anisotropyEnable = VK_FALSE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].compareEnable = VK_FALSE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].minLod = 0.f;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].maxLod = VK_LOD_CLAMP_NONE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].unnormalizedCoordinates = VK_FALSE;

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT].addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT].addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT].addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE].addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE].addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE].addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER].addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER].addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER].addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER].borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE].addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE].addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE].addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT].magFilter = VK_FILTER_LINEAR;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT].minFilter = VK_FILTER_LINEAR;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT].mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT].addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT].addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT].addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE].addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE].addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE].addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER].addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER].addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER].addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER].borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE].addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE].addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE].addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;


    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_REPEAT].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_REPEAT].maxAnisotropy = 2.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRRORED_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRRORED_REPEAT].maxAnisotropy = 2.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_EDGE].maxAnisotropy = 2.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_BORDER] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_BORDER].maxAnisotropy = 2.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRROR_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 2.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_REPEAT].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_REPEAT].maxAnisotropy = 2.f;

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRRORED_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRRORED_REPEAT].maxAnisotropy = 2.f;

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_EDGE].maxAnisotropy = 2.f;

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_BORDER] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_BORDER].maxAnisotropy = 2.f;

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRROR_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 2.f;


    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_REPEAT].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_REPEAT].maxAnisotropy = 4.f;

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRRORED_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRRORED_REPEAT].maxAnisotropy = 4.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_EDGE].maxAnisotropy = 4.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_BORDER] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_BORDER].maxAnisotropy = 4.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRROR_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 4.f;

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_REPEAT].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_REPEAT].maxAnisotropy = 4.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRRORED_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRRORED_REPEAT].maxAnisotropy = 4.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_EDGE].maxAnisotropy = 4.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_BORDER] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_BORDER].maxAnisotropy = 4.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRROR_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 4.f;


    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_REPEAT].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_REPEAT].maxAnisotropy = 8.f;

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRRORED_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRRORED_REPEAT].maxAnisotropy = 8.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_EDGE].maxAnisotropy = 8.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_BORDER] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_BORDER].maxAnisotropy = 8.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRROR_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 8.f;

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_REPEAT].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_REPEAT].maxAnisotropy = 8.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRRORED_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRRORED_REPEAT].maxAnisotropy = 8.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_EDGE].maxAnisotropy = 8.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_BORDER] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_BORDER].maxAnisotropy = 8.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRROR_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 8.f;


    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_REPEAT].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_REPEAT].maxAnisotropy = 16.f;

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRRORED_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRRORED_REPEAT].maxAnisotropy = 16.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_EDGE].maxAnisotropy = 16.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_BORDER] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_BORDER].maxAnisotropy = 16.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRROR_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 16.f;

    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT].maxAnisotropy = 16.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRRORED_REPEAT] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRRORED_REPEAT].maxAnisotropy = 16.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_EDGE].maxAnisotropy = 16.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_BORDER] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_BORDER].maxAnisotropy = 16.f;
    
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRROR_CLAMP_TO_EDGE] = smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE];
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCreateInfos[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 16.f;


    for (size_t i = 0; i < smpCreateInfos.size(); ++i) {
        s_commonSamplers[i].Create(smpCreateInfos[i]);
        CORE_ASSERT(s_commonSamplers[i].IsCreated());
        s_commonSamplers[i].SetDebugName(COMMON_SAMPLERS_DBG_NAMES[i]);
    }

    CORE_LOG_INFO("Common samplers initialization finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void WriteDescriptorSet()
{
    std::vector<VkWriteDescriptorSet> descWrites;

    std::vector<VkDescriptorImageInfo> samplerInfos(s_commonSamplers.size());
    samplerInfos.clear();

    for (size_t i = 0; i < s_commonSamplers.size(); ++i) {
        VkDescriptorImageInfo commonSamplerInfo = {};
        commonSamplerInfo.sampler = s_commonSamplers[i].Get();
        commonSamplerInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        samplerInfos.emplace_back(commonSamplerInfo);
    
        VkWriteDescriptorSet commonSamplerWrite = {};
        commonSamplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        commonSamplerWrite.dstSet = s_vkDescriptorSet;
        commonSamplerWrite.dstBinding = 0;
        commonSamplerWrite.dstArrayElement = i;
        commonSamplerWrite.descriptorCount = 1;
        commonSamplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        commonSamplerWrite.pImageInfo = &samplerInfos.back();
    
        descWrites.emplace_back(commonSamplerWrite);
    }


    VkDescriptorBufferInfo commonConstBufferInfo = {};
    commonConstBufferInfo.buffer = s_commonConstBuffer.Get();
    commonConstBufferInfo.offset = 0;
    commonConstBufferInfo.range = sizeof(COMMON_CB_DATA);

    VkWriteDescriptorSet commonConstBufWrite = {};
    commonConstBufWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    commonConstBufWrite.dstSet = s_vkDescriptorSet;
    commonConstBufWrite.dstBinding = 1;
    commonConstBufWrite.dstArrayElement = 0;
    commonConstBufWrite.descriptorCount = 1;
    commonConstBufWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    commonConstBufWrite.pBufferInfo = &commonConstBufferInfo;

    descWrites.emplace_back(commonConstBufWrite);


    VkDescriptorBufferInfo commonRenderInfoBufferInfo = {};
    commonRenderInfoBufferInfo.buffer = s_commonRenderInfoBuffer.Get();
    commonRenderInfoBufferInfo.offset = 0;
    commonRenderInfoBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet commonRenderInfoBufferWrite = {};
    commonRenderInfoBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    commonRenderInfoBufferWrite.dstSet = s_vkDescriptorSet;
    commonRenderInfoBufferWrite.dstBinding = 2;
    commonRenderInfoBufferWrite.dstArrayElement = 0;
    commonRenderInfoBufferWrite.descriptorCount = 1;
    commonRenderInfoBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    commonRenderInfoBufferWrite.pBufferInfo = &commonRenderInfoBufferInfo;

    descWrites.emplace_back(commonRenderInfoBufferWrite);


    VkDescriptorBufferInfo commonMaterialsBufferInfo = {};
    commonMaterialsBufferInfo.buffer = s_commonMaterialsBuffer.Get();
    commonMaterialsBufferInfo.offset = 0;
    commonMaterialsBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet commonMaterialsBufferWrite = {};
    commonMaterialsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    commonMaterialsBufferWrite.dstSet = s_vkDescriptorSet;
    commonMaterialsBufferWrite.dstBinding = 3;
    commonMaterialsBufferWrite.dstArrayElement = 0;
    commonMaterialsBufferWrite.descriptorCount = 1;
    commonMaterialsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    commonMaterialsBufferWrite.pBufferInfo = &commonMaterialsBufferInfo;

    descWrites.emplace_back(commonMaterialsBufferWrite);


    VkDescriptorBufferInfo commonTrsBufferInfo = {};
    commonTrsBufferInfo.buffer = s_commonTransformsBuffer.Get();
    commonTrsBufferInfo.offset = 0;
    commonTrsBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet commonTrsBufferWrite = {};
    commonTrsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    commonTrsBufferWrite.dstSet = s_vkDescriptorSet;
    commonTrsBufferWrite.dstBinding = 4;
    commonTrsBufferWrite.dstArrayElement = 0;
    commonTrsBufferWrite.descriptorCount = 1;
    commonTrsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    commonTrsBufferWrite.pBufferInfo = &commonTrsBufferInfo;

    descWrites.emplace_back(commonTrsBufferWrite);


    std::vector<VkDescriptorImageInfo> imageInfos(s_sceneImageViews.size());
    imageInfos.clear();

    for (size_t i = 0; i < s_sceneImageViews.size(); ++i) {
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageView = s_sceneImageViews[i].Get();
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imageInfos.emplace_back(imageInfo);

        VkWriteDescriptorSet texWrite = {};
        texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        texWrite.dstSet = s_vkDescriptorSet;
        texWrite.dstBinding = 5;
        texWrite.dstArrayElement = i;
        texWrite.descriptorCount = 1;
        texWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        texWrite.pImageInfo = &imageInfos.back();

        descWrites.emplace_back(texWrite);
    }

    VkDescriptorImageInfo emptyTexInfo = {};
    emptyTexInfo.imageView = s_sceneDefaultImageView.Get();
    emptyTexInfo.sampler = VK_NULL_HANDLE;
    emptyTexInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    for (size_t i = s_sceneImageViews.size(); i < 128; ++i) {
        VkWriteDescriptorSet texWrite = {};
        texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        texWrite.dstSet = s_vkDescriptorSet;
        texWrite.dstBinding = 5;
        texWrite.dstArrayElement = i;
        texWrite.descriptorCount = 1;
        texWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        texWrite.pImageInfo = &emptyTexInfo;

        descWrites.emplace_back(texWrite);
    }
    
    vkUpdateDescriptorSets(s_vkDevice.Get(), descWrites.size(), descWrites.data(), 0, nullptr);
}


static void CmdPipelineImageBarrier(
    vkn::CmdBuffer& cmdBuffer, 
    VkImageLayout oldLayout, 
    VkImageLayout newLayout,
    VkPipelineStageFlags2 srcStageMask, 
    VkPipelineStageFlags2 dstStageMask,
    VkAccessFlags2 srcAccessMask, 
    VkAccessFlags2 dstAccessMask,
    VkImage image,
    VkImageAspectFlags aspectMask
) {
    VkImageMemoryBarrier2 imageBarrier2 = {};
    imageBarrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    imageBarrier2.srcStageMask = srcStageMask;
    imageBarrier2.srcAccessMask = srcAccessMask;
    imageBarrier2.dstStageMask = dstStageMask;
    imageBarrier2.dstAccessMask = dstAccessMask;
    imageBarrier2.oldLayout = oldLayout;
    imageBarrier2.newLayout = newLayout;
    imageBarrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier2.image = image;
    imageBarrier2.subresourceRange.aspectMask = aspectMask;
    imageBarrier2.subresourceRange.baseMipLevel = 0;
    imageBarrier2.subresourceRange.baseArrayLayer = 0;
    imageBarrier2.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    imageBarrier2.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkDependencyInfo vkDependencyInfo = {};
    vkDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    vkDependencyInfo.imageMemoryBarrierCount = 1;
    vkDependencyInfo.pImageMemoryBarriers = &imageBarrier2;

    cmdBuffer.CmdPipelineBarrier2(vkDependencyInfo);
}


static void SubmitVkQueue(VkQueue vkQueue,
    VkCommandBuffer vkCmdBuffer,
    VkFence vkFinishFence,
    VkSemaphore vkWaitSemaphore, 
    VkPipelineStageFlags2 waitSemaphoreStageMask,
    VkSemaphore vkSignalSemaphore, 
    VkPipelineStageFlags2 signalSemaphoreStageMask
) {
    VkSemaphoreSubmitInfo waitSemaphoreInfo = {};
    waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSemaphoreInfo.semaphore = vkWaitSemaphore;
    waitSemaphoreInfo.value = 0;
    waitSemaphoreInfo.stageMask = waitSemaphoreStageMask;
    waitSemaphoreInfo.deviceIndex = 0;

    VkSemaphoreSubmitInfo signalSemaphoreInfo = {};
    signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphoreInfo.semaphore = vkSignalSemaphore;
    signalSemaphoreInfo.value = 0;
    signalSemaphoreInfo.stageMask = signalSemaphoreStageMask;
    signalSemaphoreInfo.deviceIndex = 0;
    
    VkCommandBufferSubmitInfo commandBufferInfo = {};
    commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandBufferInfo.commandBuffer = vkCmdBuffer;
    commandBufferInfo.deviceMask = 0;

    VkSubmitInfo2 submitInfo2 = {};
    submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo2.waitSemaphoreInfoCount = vkWaitSemaphore != VK_NULL_HANDLE ? 1 : 0;
    submitInfo2.pWaitSemaphoreInfos = &waitSemaphoreInfo;
    submitInfo2.commandBufferInfoCount = 1;
    submitInfo2.pCommandBufferInfos = &commandBufferInfo;
    submitInfo2.signalSemaphoreInfoCount = vkSignalSemaphore != VK_NULL_HANDLE ? 1 : 0;
    submitInfo2.pSignalSemaphoreInfos = &signalSemaphoreInfo;

    VK_CHECK(vkQueueSubmit2(vkQueue, 1, &submitInfo2, vkFinishFence));
}


template <typename Func, typename... Args>
static void ImmediateSubmitQueue(VkQueue vkQueue, Func func, Args&&... args)
{   
    s_vkImmediateSubmitFinishedFence.Reset();
    s_vkImmediateSubmitCmdBuffer.Reset();

    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    s_vkImmediateSubmitCmdBuffer.Begin(cmdBeginInfo);
        func(s_vkImmediateSubmitCmdBuffer, std::forward<Args>(args)...);
    s_vkImmediateSubmitCmdBuffer.End();

    SubmitVkQueue(
        vkQueue, 
        s_vkImmediateSubmitCmdBuffer.Get(), 
        s_vkImmediateSubmitFinishedFence.Get(), 
        VK_NULL_HANDLE, 
        VK_PIPELINE_STAGE_2_NONE,
        VK_NULL_HANDLE, 
        VK_PIPELINE_STAGE_2_NONE
    );

    s_vkImmediateSubmitFinishedFence.WaitFor(10'000'000'000);
}


void UpdateCommonConstBuffer(Window* pWnd)
{
    ENG_PROFILE_SCOPED_MARKER_C("UpdateCommonConstBuffer", 200, 200, 0, 255);

    const glm::mat4x4 viewMat = s_camera.GetViewMatrix();
    
    #ifdef ENG_REVERSED_Z
        glm::mat4x4 projMat = glm::perspective(glm::radians(90.f), (float)pWnd->GetWidth() / pWnd->GetHeight(), 100'000.f, 0.01f);
    #else
        glm::mat4x4 projMat = glm::perspective(glm::radians(90.f), (float)pWnd->GetWidth() / pWnd->GetHeight(), 0.01f, 100'000.f);
    #endif
    projMat[1][1] *= -1.f;

    COMMON_CB_DATA* pCommonConstBufferData = static_cast<COMMON_CB_DATA*>(s_commonConstBuffer.Map(0, VK_WHOLE_SIZE, 0));

    pCommonConstBufferData->COMMON_VIEW_MATRIX = viewMat;
    pCommonConstBufferData->COMMON_PROJ_MATRIX = projMat;
    pCommonConstBufferData->COMMON_VIEW_PROJ_MATRIX = projMat * viewMat;
    
    uint32_t dbgFlags = 0;
    
    switch(s_dbgTexIdx) {
        case 0:
            dbgFlags |= (uint32_t)COMMON_DBG_FLAG_MASKS::OUTPUT_COMMON_MTL_ALBEDO_TEX;
            break;
        case 1:
            dbgFlags |= (uint32_t)COMMON_DBG_FLAG_MASKS::OUTPUT_COMMON_MTL_NORMAL_TEX;
            break;
        case 2:
            dbgFlags |= (uint32_t)COMMON_DBG_FLAG_MASKS::OUTPUT_COMMON_MTL_MR_TEX;
            break;
        case 3:
            dbgFlags |= (uint32_t)COMMON_DBG_FLAG_MASKS::OUTPUT_COMMON_MTL_AO_TEX;
            break;
        case 4:
            dbgFlags |= (uint32_t)COMMON_DBG_FLAG_MASKS::OUTPUT_COMMON_MTL_EMISSIVE_TEX;
            break;
        default:
            CORE_ASSERT_FAIL("Invalid material debug texture viewer index: %u", s_dbgTexIdx);
            break;
    }
    
    pCommonConstBufferData->COMMON_DBG_FLAGS = dbgFlags;

    s_commonConstBuffer.Unmap();
}


void RenderScene()
{
    ENG_PROFILE_SCOPED_MARKER_C("RenderScene", 255, 255, 0, 255);

    vkn::Fence& renderingFinishedFence = s_vkRenderingFinishedFence;

    const VkResult fenceStatus = vkGetFenceStatus(s_vkDevice.Get(), renderingFinishedFence.Get());
    if (fenceStatus == VK_NOT_READY) {
        return;
    } else {
        renderingFinishedFence.Reset();
    }

    UpdateCommonConstBuffer(s_pWnd);

    vkn::Semaphore& presentFinishedSemaphore = s_vkPresentFinishedSemaphore;

    uint32_t nextImageIdx;
    const VkResult acquireResult = vkAcquireNextImageKHR(s_vkDevice.Get(), s_vkSwapchain.Get(), 10'000'000'000, presentFinishedSemaphore.Get(), VK_NULL_HANDLE, &nextImageIdx);
    
    if (acquireResult != VK_SUBOPTIMAL_KHR && acquireResult != VK_ERROR_OUT_OF_DATE_KHR) {
        VK_CHECK(acquireResult);
    } else {
        s_swapchainRecreateRequired = true;
        return;
    }

    vkn::Semaphore& renderingFinishedSemaphore = s_vkRenderingFinishedSemaphores[nextImageIdx];
    vkn::CmdBuffer& cmdBuffer = s_vkRenderCmdBuffer;

    cmdBuffer.Reset();

    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkImage rndImage = s_vkSwapchain.GetImage(nextImageIdx);

    cmdBuffer.Begin(cmdBeginInfo);
        ENG_PROFILE_BEGIN_GPU_MARKER_C_SCOPE(cmdBuffer, "BeginCmdBuffer", 255, 165, 0, 255);

        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            rndImage,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            s_vkDepthImage.Get(),
            VK_IMAGE_ASPECT_DEPTH_BIT
        );

        VkRenderingInfo renderingInfo = {};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.extent = s_vkSwapchain.GetImageExtent();
        renderingInfo.renderArea.offset = {0, 0};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;

        VkRenderingAttachmentInfo colorAttachment = {};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = s_vkSwapchain.GetImageView(nextImageIdx);
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color.float32[0] = 245.f / 255.f;
        colorAttachment.clearValue.color.float32[1] = 245.f / 255.f;
        colorAttachment.clearValue.color.float32[2] = 220.f / 255.f;
        colorAttachment.clearValue.color.float32[3] = 255.f / 255.f;
        renderingInfo.pColorAttachments = &colorAttachment;

        VkRenderingAttachmentInfo depthAttachment = {};
        depthAttachment.sType     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = s_vkDepthImageView.Get();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    #ifdef ENG_REVERSED_Z
        depthAttachment.clearValue.depthStencil.depth = 0.f;
    #else
        depthAttachment.clearValue.depthStencil.depth = 1.f;
    #endif

        renderingInfo.pDepthAttachment = &depthAttachment;

        ENG_PROFILE_BEGIN_GPU_MARKER_C_SCOPE(cmdBuffer, "BeginRender", 200, 120, 0, 255);
        cmdBuffer.CmdBeginRendering(renderingInfo);
            VkViewport viewport = {};
            viewport.width = renderingInfo.renderArea.extent.width;
            viewport.height = renderingInfo.renderArea.extent.height;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            cmdBuffer.CmdSetViewport(0, 1, &viewport);

            VkRect2D scissor = {};
            scissor.extent = renderingInfo.renderArea.extent;
            cmdBuffer.CmdSetScissor(0, 1, &scissor);

            vkCmdBindPipeline(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, s_vkPipeline);
            
            vkCmdBindDescriptorSets(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, s_vkPipelineLayout, 0, 1, &s_vkDescriptorSet, 0, nullptr);

            cmdBuffer.CmdBindIndexBuffer(s_indexBuffer, 0, GetVkIndexType());

            for (uint32_t rndInfoIdx = 0; rndInfoIdx < s_sceneRenderInfos.size(); ++rndInfoIdx) {
                TEST_BINDLESS_REGISTRY registry = {};
                registry.VERTEX_DATA = s_vertexBuffer.GetDeviceAddress();
                registry.RENDER_INFO_IDX = rndInfoIdx;

                vkCmdPushConstants(cmdBuffer.Get(), s_vkPipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(TEST_BINDLESS_REGISTRY), &registry);

                const Mesh& mesh = s_sceneMeshes[s_sceneRenderInfos[rndInfoIdx].MESH_IDX];

                cmdBuffer.CmdDrawIndexed(mesh.indexCount, 1, mesh.firstIndex, mesh.firstVertex, 0);
            }
        cmdBuffer.CmdEndRendering();
        ENG_PROFILE_END_GPU_MARKER_SCOPE(cmdBuffer);

        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_2_NONE,
            rndImage,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        ENG_PROFILE_END_GPU_MARKER_SCOPE(cmdBuffer);

        ENG_PROFILE_GPU_COLLECT_STATS(cmdBuffer);
    cmdBuffer.End();

    SubmitVkQueue(
        s_vkDevice.GetQueue(),
        cmdBuffer.Get(),
        renderingFinishedFence.Get(),
        presentFinishedSemaphore.Get(),
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        renderingFinishedSemaphore.Get(),
        VK_PIPELINE_STAGE_2_NONE
    );

    ENG_PROFILE_BEGIN_MARKER_C_SCOPE("Presenting", 200, 200, 0, 255);
    VkSwapchainKHR vkSwapchain = s_vkSwapchain.Get();
    VkSemaphore vkWaitSemaphore = renderingFinishedSemaphore.Get();

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &vkWaitSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &vkSwapchain;
    presentInfo.pImageIndices = &nextImageIdx;
    presentInfo.pResults = nullptr;
    const VkResult presentResult = vkQueuePresentKHR(s_vkDevice.GetQueue(), &presentInfo);

    if (presentResult != VK_SUBOPTIMAL_KHR && presentResult != VK_ERROR_OUT_OF_DATE_KHR) {
        VK_CHECK(presentResult);
    } else {
        s_swapchainRecreateRequired = true;
        return;
    }
    ENG_PROFILE_END_MARKER_SCOPE();
}


static void LoadSceneMaterials(const gltf::Model& model)
{
    ENG_PROFILE_SCOPED_MARKER_C("LoadSceneMaterials", 225, 0, 225, 255);

    Timer timer;

    s_sceneMaterials.resize(model.materials.size());
    s_sceneMaterials.clear();

    s_sceneImages.resize(model.images.size());
    s_sceneImageViews.resize(model.images.size());

    std::vector<vkn::Buffer> stagingSceneImageBuffers(model.images.size());

    auto AddGltfMaterialTexture = [&stagingSceneImageBuffers, &model](int32_t texIdx, bool isSRGB = false) -> void
    {
        if (texIdx == -1 || s_sceneImages[texIdx].IsCreated()) {
            return;
        }

        const gltf::Image& gltfImage = model.images[texIdx];

        vkn::BufferCreateInfo stagingTexBufCreateInfo = {};
        stagingTexBufCreateInfo.pDevice = &s_vkDevice;
        stagingTexBufCreateInfo.size = gltfImage.image.size() * sizeof(gltfImage.image[0]);
        stagingTexBufCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingTexBufCreateInfo.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        stagingTexBufCreateInfo.memAllocFlags = 0;

        vkn::Buffer& stagingTexBuffer = stagingSceneImageBuffers[texIdx];
        stagingTexBuffer.Create(stagingTexBufCreateInfo);
        CORE_ASSERT(stagingTexBuffer.IsCreated());

        void* pImageData = stagingTexBuffer.Map(0, VK_WHOLE_SIZE, 0);
        memcpy(pImageData, gltfImage.image.data(), stagingTexBufCreateInfo.size);
        stagingTexBuffer.Unmap();

        vkn::ImageCreateInfo info = {};

        info.pDevice = &s_vkDevice;
        info.type = VK_IMAGE_TYPE_2D;
        info.extent.width = gltfImage.width;
        info.extent.height = gltfImage.height;
        info.extent.depth = 1;
        info.format = gltf::GetImageVkFormat(gltfImage.component, gltfImage.pixel_type, isSRGB);
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.memAllocInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        vkn::Image& sceneImage = s_sceneImages[texIdx];
        sceneImage.Create(info);
        CORE_ASSERT(sceneImage.IsCreated());
        sceneImage.SetDebugName("COMMON_MTL_TEXTURE_%zu", texIdx);

        vkn::ImageViewCreateInfo viewInfo = {};

        viewInfo.pOwner = &sceneImage;
        viewInfo.type = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = info.format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        viewInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        vkn::ImageView& view = s_sceneImageViews[texIdx];
        view.Create(viewInfo);
        CORE_ASSERT(view.IsCreated());
        view.SetDebugName("COMMON_MTL_TEXTURE_VIEW_%zu", texIdx);
    };

    for (const gltf::Material& mtl : model.materials) {
        COMMON_MATERIAL material = {};

        const int32_t albedoTexIdx   = mtl.pbrMetallicRoughness.baseColorTexture.index;
        const int32_t normalTexIdx   = mtl.normalTexture.index;
        const int32_t mrTexIdx       = mtl.pbrMetallicRoughness.metallicRoughnessTexture.index;
        const int32_t aoTexIdx       = mtl.occlusionTexture.index;
        const int32_t emissiveTexIdx = mtl.emissiveTexture.index;

        material.ALBEDO_TEX_IDX   = albedoTexIdx >= 0 ? model.textures[albedoTexIdx].source : -1;
        material.NORMAL_TEX_IDX   = normalTexIdx >= 0 ? model.textures[normalTexIdx].source : -1;
        material.MR_TEX_IDX       = mrTexIdx >= 0 ? model.textures[mrTexIdx].source : -1;
        material.AO_TEX_IDX       = aoTexIdx >= 0 ? model.textures[aoTexIdx].source : -1;
        material.EMISSIVE_TEX_IDX = emissiveTexIdx >= 0 ? model.textures[emissiveTexIdx].source : -1;
    
        s_sceneMaterials.emplace_back(material);

        AddGltfMaterialTexture(material.ALBEDO_TEX_IDX, true);
        AddGltfMaterialTexture(material.NORMAL_TEX_IDX);
        AddGltfMaterialTexture(material.MR_TEX_IDX);
        AddGltfMaterialTexture(material.AO_TEX_IDX);
        AddGltfMaterialTexture(material.EMISSIVE_TEX_IDX, true);
    }

    vkn::BufferCreateInfo commonMtlBuffCreateInfo = {};
    commonMtlBuffCreateInfo.pDevice = &s_vkDevice;
    commonMtlBuffCreateInfo.size = s_sceneMaterials.size() * sizeof(COMMON_MATERIAL);
    commonMtlBuffCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    commonMtlBuffCreateInfo.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    s_commonMaterialsBuffer.Create(commonMtlBuffCreateInfo);
    CORE_ASSERT(s_commonMaterialsBuffer.IsCreated());
    s_commonMaterialsBuffer.SetDebugName("COMMON_MATERIALS");

    void* pCommonMaterialsData = s_commonMaterialsBuffer.Map(0, VK_WHOLE_SIZE, 0);
    memcpy(pCommonMaterialsData, s_sceneMaterials.data(), s_sceneMaterials.size() * sizeof(COMMON_MATERIAL));
    s_commonMaterialsBuffer.Unmap();

    vkn::ImageCreateInfo defTexInfo = {};

    defTexInfo.pDevice = &s_vkDevice;
    defTexInfo.type = VK_IMAGE_TYPE_2D;
    defTexInfo.extent.width = 1;
    defTexInfo.extent.height = 1;
    defTexInfo.extent.depth = 1;
    defTexInfo.format = VK_FORMAT_R8_UNORM;
    defTexInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    defTexInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    defTexInfo.mipLevels = 1;
    defTexInfo.arrayLayers = 1;
    defTexInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    defTexInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    defTexInfo.memAllocInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    s_sceneDefaultImage.Create(defTexInfo);
    CORE_ASSERT(s_sceneDefaultImage.IsCreated());
    s_sceneDefaultImage.SetDebugName("DEFAULT_TEX");

    vkn::ImageViewCreateInfo defTexViewInfo = {};

    defTexViewInfo.pOwner = &s_sceneDefaultImage;
    defTexViewInfo.type = VK_IMAGE_VIEW_TYPE_2D;
    defTexViewInfo.format = defTexInfo.format;
    defTexViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    defTexViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    defTexViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    defTexViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    defTexViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    defTexViewInfo.subresourceRange.baseMipLevel = 0;
    defTexViewInfo.subresourceRange.baseArrayLayer = 0;
    defTexViewInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    defTexViewInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    s_sceneDefaultImageView.Create(defTexViewInfo);
    CORE_ASSERT(s_sceneDefaultImageView.IsCreated());
    s_sceneDefaultImageView.SetDebugName("DEFAULT_TEX_VIEW");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        for (size_t i = 0; i < s_sceneImages.size(); ++i) {
            if (!s_sceneImages[i].IsCreated()) {
                continue;
            }

            CmdPipelineImageBarrier(
                cmdBuffer,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_NONE,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_ACCESS_2_NONE,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                s_sceneImages[i].Get(),
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            VkCopyBufferToImageInfo2 copyInfo = {};

            copyInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
            copyInfo.srcBuffer = stagingSceneImageBuffers[i].Get();
            copyInfo.dstImage = s_sceneImages[i].Get();
            copyInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            copyInfo.regionCount = 1;

            VkBufferImageCopy2 texRegion = {};

            texRegion.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
            texRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            texRegion.imageSubresource.mipLevel = 0;
            texRegion.imageSubresource.baseArrayLayer = 0;
            texRegion.imageSubresource.layerCount = 1;
            texRegion.imageExtent = s_sceneImages[i].GetExtent();

            copyInfo.pRegions = &texRegion;

            vkCmdCopyBufferToImage2(cmdBuffer.Get(), &copyInfo);

            CmdPipelineImageBarrier(
                cmdBuffer,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_ACCESS_2_SHADER_READ_BIT,
                s_sceneImages[i].Get(),
                VK_IMAGE_ASPECT_COLOR_BIT
            );
        }
    
        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_SHADER_READ_BIT,
            s_sceneDefaultImage.Get(),
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    });

    for (vkn::Buffer& buffer : stagingSceneImageBuffers) {
        buffer.Destroy();
    }

    CORE_LOG_INFO("Materials loading finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void LoadSceneMeshes(const gltf::Model& model)
{
    ENG_PROFILE_SCOPED_MARKER_C("LoadSceneMeshes", 225, 0, 225, 255);
    
    Timer timer;

    size_t vertexCount = 0;
    std::for_each(model.meshes.cbegin(), model.meshes.cend(), [&model, &vertexCount](const gltf::Mesh& mesh){
        std::for_each(mesh.primitives.cbegin(), mesh.primitives.cend(), [&model, &vertexCount](const gltf::Primitive& primitive){
            CORE_ASSERT(primitive.attributes.contains("POSITION"));
            const uint32_t positionAccessorIndex = primitive.attributes.at("POSITION");
            const gltf::Accessor& positionAccessor = model.accessors[positionAccessorIndex];

            vertexCount += positionAccessor.count;
        });
    });

    CORE_ASSERT_MSG(vertexCount < MAX_VERTEX_COUNT, "Vertex buffer overflow: %zu, max vertex count: %zu", vertexCount, MAX_VERTEX_COUNT);

    std::vector<Vertex> cpuVertBuffer;
    cpuVertBuffer.reserve(vertexCount);

    size_t indexCount = 0;
    std::for_each(model.meshes.cbegin(), model.meshes.cend(), [&model, &indexCount](const gltf::Mesh& mesh){
        std::for_each(mesh.primitives.cbegin(), mesh.primitives.cend(), [&model, &indexCount](const gltf::Primitive& primitive){
            if (primitive.indices >= 0) {
                const gltf::Accessor& indexAccessor = model.accessors[primitive.indices];

                indexCount += indexAccessor.count;
            }
        });
    });

    CORE_ASSERT_MSG(indexCount < MAX_INDEX_COUNT, "Index buffer overflow: %zu, max index count: %zu", indexCount, MAX_INDEX_COUNT);

    std::vector<VertexIndexType> cpuIndexBuffer;
    cpuIndexBuffer.reserve(indexCount);

    s_sceneRenderInfos.reserve(model.meshes.size());
    s_sceneRenderInfos.clear();

    s_sceneMeshes.reserve(model.meshes.size());
    s_sceneMeshes.clear();

    for (const gltf::Mesh& m : model.meshes) {
        for (const gltf::Primitive& primitive : m.primitives) {
            COMMON_RENDER_INFO renderInfo = {};
            renderInfo.MESH_IDX = s_sceneMeshes.size();
            renderInfo.MATERIAL_IDX = primitive.material;

            Mesh mesh = {};

            mesh.firstVertex = cpuVertBuffer.size();
            mesh.firstIndex = cpuIndexBuffer.size();

            const VertexIndexType primitiveStartIndex = cpuVertBuffer.size();

            CORE_ASSERT(primitive.attributes.contains("POSITION"));
            const uint32_t positionAccessorIndex = primitive.attributes.at("POSITION");
            const gltf::Accessor& positionAccessor  = model.accessors[positionAccessorIndex];
            
            const auto& positionBufferView = model.bufferViews[positionAccessor.bufferView];
            const auto& positionBuffer = model.buffers[positionBufferView.buffer];

            const uint8_t* pPositionData = positionBuffer.data.data() + positionBufferView.byteOffset + positionAccessor.byteOffset;

            CORE_ASSERT(primitive.attributes.contains("NORMAL"));
            const uint32_t normalAccessorIndex = primitive.attributes.at("NORMAL");
            const gltf::Accessor& normalAccessor = model.accessors[normalAccessorIndex];

            const auto& normalBufferView = model.bufferViews[normalAccessor.bufferView];
            const auto& normalBuffer = model.buffers[normalBufferView.buffer];

            const uint8_t* pNormalData = normalBuffer.data.data() + normalBufferView.byteOffset + normalAccessor.byteOffset;

            CORE_ASSERT(primitive.attributes.contains("TEXCOORD_0"));
            const uint32_t texcoordAccessorIndex = primitive.attributes.at("TEXCOORD_0");
            const gltf::Accessor& texcoordAccessor = model.accessors[texcoordAccessorIndex];

            const auto& texcoordBufferView = model.bufferViews[texcoordAccessor.bufferView];
            const auto& texcoordBuffer = model.buffers[texcoordBufferView.buffer];

            const uint8_t* pTexcoordData = texcoordBuffer.data.data() + texcoordBufferView.byteOffset + texcoordAccessor.byteOffset;

            mesh.vertexCount += positionAccessor.count;

            for (size_t i = 0; i < positionAccessor.count; ++i) {
                const float* pPosition = reinterpret_cast<const float*>(pPositionData + i * positionAccessor.ByteStride(positionBufferView));
                const float* pNormal = reinterpret_cast<const float*>(pNormalData + i * normalAccessor.ByteStride(normalBufferView));
                const float* pTexcoord = reinterpret_cast<const float*>(pTexcoordData + i * texcoordAccessor.ByteStride(texcoordBufferView));
                
                Vertex vertex = {};

                vertex.posXY = glm::packHalf2x16(glm::vec2(pPosition[0], pPosition[1]));
                vertex.posZnormX = glm::packHalf2x16(glm::vec2(pPosition[2], pNormal[0]));
                vertex.normYZ = glm::packHalf2x16(glm::vec2(pNormal[1], pNormal[2]));
                vertex.texcoord = glm::packHalf2x16(glm::vec2(pTexcoord[0], pTexcoord[1]));

                cpuVertBuffer.emplace_back(vertex);
            }

            CORE_ASSERT_MSG(primitive.indices >= 0, "GLTF primitive must have index accessor");

            const gltf::Accessor& indexAccessor = model.accessors[primitive.indices];
            const gltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
            const gltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

            const uint8_t* pIndexData = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;

            mesh.indexCount += indexAccessor.count;

            for (size_t i = 0; i < indexAccessor.count; ++i) {
                uint32_t index = UINT32_MAX;

                switch (indexAccessor.componentType) {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                        index = static_cast<uint32_t>(*(reinterpret_cast<const uint8_t*>(pIndexData + i)));
                        break;
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                        index = static_cast<uint32_t>(*(reinterpret_cast<const uint16_t*>(pIndexData + i * 2)));
                        break;
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                        index = static_cast<uint32_t>(*(reinterpret_cast<const uint32_t*>(pIndexData + i * 4)));
                        break;
                    default:
                        CORE_ASSERT_FAIL("Invalid GLTF index type: %d", indexAccessor.componentType);
                        break;
                }

                index = primitiveStartIndex + index;

                CORE_ASSERT_MSG(index < std::numeric_limits<VertexIndexType>::max(), "Vertex index is greater than %zu", std::numeric_limits<VertexIndexType>::max());
                cpuIndexBuffer.push_back(static_cast<VertexIndexType>(index));
            }

            s_sceneMeshes.emplace_back(mesh);
            s_sceneRenderInfos.emplace_back(renderInfo);
        }
    }


    vkn::BufferCreateInfo stagingBufCreateInfo = {};
    stagingBufCreateInfo.pDevice = &s_vkDevice;
    stagingBufCreateInfo.size = cpuVertBuffer.size() * sizeof(Vertex);
    stagingBufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufCreateInfo.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    stagingBufCreateInfo.memAllocFlags = 0;

    vkn::Buffer stagingVertBuffer(stagingBufCreateInfo);
    CORE_ASSERT(stagingVertBuffer.IsCreated());
    stagingVertBuffer.SetDebugName("STAGING_VERT_BUFFER");

    stagingBufCreateInfo.size = cpuIndexBuffer.size() * sizeof(VertexIndexType);

    vkn::Buffer stagingIndexBuffer(stagingBufCreateInfo);
    CORE_ASSERT(stagingIndexBuffer.IsCreated());
    stagingIndexBuffer.SetDebugName("STAGING_IDX_BUFFER");

    {
        void* pVertexBufferData = stagingVertBuffer.Map(0, VK_WHOLE_SIZE, 0);
        memcpy(pVertexBufferData, cpuVertBuffer.data(), cpuVertBuffer.size() * sizeof(cpuVertBuffer[0]));
        stagingVertBuffer.Unmap();
    }

    {
        void* pIndexBufferData = stagingIndexBuffer.Map(0, VK_WHOLE_SIZE, 0);
        memcpy(pIndexBufferData, cpuIndexBuffer.data(), cpuIndexBuffer.size() * sizeof(cpuIndexBuffer[0]));
        stagingIndexBuffer.Unmap();
    }

    vkn::BufferCreateInfo vertBufCreateInfo = {};
    vertBufCreateInfo.pDevice = &s_vkDevice;
    vertBufCreateInfo.size = VERTEX_BUFFER_SIZE_BYTES;
    vertBufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertBufCreateInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    vertBufCreateInfo.memAllocFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    s_vertexBuffer.Create(vertBufCreateInfo);
    CORE_ASSERT(s_vertexBuffer.IsCreated());
    s_vertexBuffer.SetDebugName("COMMON_VB");


    vkn::BufferCreateInfo idxBufCreateInfo = {};
    idxBufCreateInfo.pDevice = &s_vkDevice;
    idxBufCreateInfo.size = INDEX_BUFFER_SIZE_BYTES;
    idxBufCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    idxBufCreateInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    idxBufCreateInfo.memAllocFlags = 0;

    s_indexBuffer.Create(idxBufCreateInfo);
    CORE_ASSERT(s_indexBuffer.IsCreated());
    s_indexBuffer.SetDebugName("COMMON_IB");


    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        VkBufferCopy bufferRegion = {};
        
        bufferRegion.size = cpuVertBuffer.size() * sizeof(cpuVertBuffer[0]);
        vkCmdCopyBuffer(cmdBuffer.Get(), stagingVertBuffer.Get(), s_vertexBuffer.Get(), 1, &bufferRegion);

        bufferRegion.size = cpuIndexBuffer.size() * sizeof(cpuIndexBuffer[0]);
        vkCmdCopyBuffer(cmdBuffer.Get(), stagingIndexBuffer.Get(), s_indexBuffer.Get(), 1, &bufferRegion);
    });

    stagingVertBuffer.Destroy();
    stagingIndexBuffer.Destroy();

    CORE_LOG_INFO("Mesh loading finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void LoadTransforms(const gltf::Model& model)
{
    ENG_PROFILE_SCOPED_MARKER_C("LoadTransforms", 225, 0, 225, 255);
    
    Timer timer;

    s_sceneTransforms.resize(model.nodes.size());

    for (size_t trsIdx = 0; trsIdx < s_sceneTransforms.size(); ++trsIdx) {
        const gltf::Node& node = model.nodes[trsIdx];

        glm::mat4x4 transform(1.f);

        if (!node.matrix.empty()) {
            for (size_t rawIdx = 0; rawIdx < 4; ++rawIdx) {
                for (size_t colIdx = 0; colIdx < 4; ++colIdx) {
                    transform[rawIdx][colIdx] = node.matrix[rawIdx * 4 + colIdx];
                }
            }
        } else {
            const glm::quat rotation = node.rotation.empty() ? glm::identity<glm::quat>()
                : glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);

            const glm::vec3 scale = node.scale.empty() ? glm::vec3(1.f)
                : glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
                
            const glm::vec3 translation = node.translation.empty() ? glm::vec3(0.f)
                : glm::vec3(node.translation[0], node.translation[1], node.translation[2]);

            transform = glm::translate(transform, translation);
            transform = transform * glm::mat4_cast(rotation);
            transform = transform * glm::scale(glm::mat4x4(1.0f), scale);
            transform = glm::transpose(transform);
        }

        for (size_t i = 0; i < _countof(COMMON_TRANSFORM::MATR); ++i) {
            s_sceneTransforms[trsIdx].MATR[i] = transform[i];
        }
    }

    size_t renderInfoIdx = 0;

    for (size_t meshGroupIdx = 0; meshGroupIdx < model.meshes.size(); ++meshGroupIdx) {
        const gltf::Mesh& mesh = model.meshes[meshGroupIdx];

        for (size_t meshIdx = 0; meshIdx < mesh.primitives.size(); ++meshIdx) {
            s_sceneRenderInfos[renderInfoIdx].TRANSFORM_IDX = meshGroupIdx;
            ++renderInfoIdx;
        }
    }

    vkn::BufferCreateInfo commonTrsCreateInfo = {};
    commonTrsCreateInfo.pDevice = &s_vkDevice;
    commonTrsCreateInfo.size = s_sceneTransforms.size() * sizeof(COMMON_TRANSFORM);
    commonTrsCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    commonTrsCreateInfo.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    s_commonTransformsBuffer.Create(commonTrsCreateInfo);
    CORE_ASSERT(s_commonTransformsBuffer.IsCreated());
    s_commonTransformsBuffer.SetDebugName("COMMON_TRANSFORMS");

    void* pCommonRenderInfoData = s_commonTransformsBuffer.Map(0, VK_WHOLE_SIZE, 0);
    memcpy(pCommonRenderInfoData, s_sceneTransforms.data(), s_sceneTransforms.size() * sizeof(COMMON_TRANSFORM));
    s_commonTransformsBuffer.Unmap();

    CORE_LOG_INFO("Transforms loading finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void LoadScene(const fs::path& filepath)
{
    ENG_PROFILE_SCOPED_MARKER_C("LoadScene", 255, 0, 255, 255);

    Timer timer;

    const fs::path dirPath = filepath.parent_path();

    const std::string pathS = filepath.string();
    CORE_LOG_TRACE("Loading \"%s\"...", pathS.c_str());

    gltf::TinyGLTF modelLoader;
    gltf::Model model;
    std::string error, warning;

    const bool isModelLoaded = filepath.extension() == ".gltf" ? 
        modelLoader.LoadASCIIFromFile(&model, &error, &warning, pathS) :
        modelLoader.LoadBinaryFromFile(&model, &error, &warning, pathS);

    if (!warning.empty()) {
        CORE_LOG_WARN("Warning during %s model loading: %s", pathS.c_str(), warning.c_str());
    }
    CORE_ASSERT_MSG(isModelLoaded && error.empty(), "Failed to load %s model: %s", pathS.c_str(), error.c_str());


    LoadSceneMaterials(model);
    LoadSceneMeshes(model);
    LoadTransforms(model);


    vkn::BufferCreateInfo commonRenderInfoCreateInfo = {};
    commonRenderInfoCreateInfo.pDevice = &s_vkDevice;
    commonRenderInfoCreateInfo.size = s_sceneRenderInfos.size() * sizeof(COMMON_RENDER_INFO);
    commonRenderInfoCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    commonRenderInfoCreateInfo.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    s_commonRenderInfoBuffer.Create(commonRenderInfoCreateInfo);
    CORE_ASSERT(s_commonRenderInfoBuffer.IsCreated());
    s_commonRenderInfoBuffer.SetDebugName("COMMON_RENDER_INFOS");

    void* pCommonRenderInfoData = s_commonRenderInfoBuffer.Map(0, VK_WHOLE_SIZE, 0);
    memcpy(pCommonRenderInfoData, s_sceneRenderInfos.data(), s_sceneRenderInfos.size() * sizeof(COMMON_RENDER_INFO));
    s_commonRenderInfoBuffer.Unmap();

    
    vkn::BufferCreateInfo commonConstBufCreateInfo = {};
    commonConstBufCreateInfo.pDevice = &s_vkDevice;
    commonConstBufCreateInfo.size = sizeof(COMMON_CB_DATA);
    commonConstBufCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    commonConstBufCreateInfo.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    commonConstBufCreateInfo.memAllocFlags = 0;

    s_commonConstBuffer.Create(commonConstBufCreateInfo); 
    CORE_ASSERT(s_commonConstBuffer.IsCreated());
    s_commonConstBuffer.SetDebugName("COMMON_CB");


    CORE_LOG_INFO("\"%s\" loading finished: %f ms", pathS.c_str(), timer.End().GetDuration<float, std::milli>());
}


static void CreateDepthImage()
{
    vkn::Image& depthImage = s_vkDepthImage;
    vkn::ImageView& depthImageView = s_vkDepthImageView;

    if (depthImageView.IsCreated()) {
        depthImageView.Destroy();
    }

    if (depthImage.IsCreated()) {
        depthImage.Destroy();
    }

    vkn::ImageCreateInfo depthImageCreateInfo = {};
    depthImageCreateInfo.pDevice = &s_vkDevice;

    depthImageCreateInfo.type = VK_IMAGE_TYPE_2D;
    depthImageCreateInfo.extent = VkExtent3D{s_pWnd->GetWidth(), s_pWnd->GetHeight(), 1};
    depthImageCreateInfo.format = VK_FORMAT_D32_SFLOAT;
    depthImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; 
    depthImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthImageCreateInfo.flags = 0;
    depthImageCreateInfo.mipLevels = 1;
    depthImageCreateInfo.arrayLayers = 1;
    depthImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    
    depthImageCreateInfo.memAllocInfo.flags = 0;
    depthImageCreateInfo.memAllocInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    depthImage.Create(depthImageCreateInfo);
    CORE_ASSERT(depthImage.IsCreated());
    depthImage.SetDebugName("COMMON_DEPTH");

    vkn::ImageViewCreateInfo depthImageViewCreateInfo = {};
    depthImageViewCreateInfo.pOwner = &depthImage;
    depthImageViewCreateInfo.type = VK_IMAGE_VIEW_TYPE_2D;
    depthImageViewCreateInfo.format = VK_FORMAT_D32_SFLOAT;
    depthImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    depthImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    depthImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    depthImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    depthImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    depthImageViewCreateInfo.subresourceRange.levelCount = 1;
    depthImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    depthImageViewCreateInfo.subresourceRange.layerCount = 1;

    depthImageView.Create(depthImageViewCreateInfo);
    CORE_ASSERT(depthImageView.IsValid());
    depthImageView.SetDebugName("COMMON_DEPTH_VIEW");
}


void UpdateTimings(Window* pWnd, Timer& cpuTimer)
{
#if defined(ENG_BUILD_DEBUG)
    constexpr const char* BUILD_TYPE_STR = "DEBUG";
#elif defined(ENG_BUILD_PROFILE)
    constexpr const char* BUILD_TYPE_STR = "PROFILE";
#else
    constexpr const char* BUILD_TYPE_STR = "RELEASE";
#endif
        
    const float cpuFrameTime = cpuTimer.End().GetDuration<float, std::milli>();

    pWnd->SetTitle("%s | %s | CPU: %.3f ms (%.1f FPS) | Fly Camera Mode (F5): %s | DBG TEX: %s", 
        APP_NAME, BUILD_TYPE_STR, cpuFrameTime, 1000.f / cpuFrameTime, s_flyCameraMode ? "ON" : "OFF", DBG_TEX_OUTPUT_NAMES[s_dbgTexIdx]);
}


static bool ResizeVkSwapchain(Window* pWnd)
{
    if (!s_swapchainRecreateRequired) {
        return false;
    }

    const bool resizeResult = s_vkSwapchain.Resize(pWnd->GetWidth(), pWnd->GetHeight());
    
    s_swapchainRecreateRequired = !resizeResult;

    return s_swapchainRecreateRequired;
}


static void CameraProcessWndEvent(eng::Camera& camera, const WndEvent& event)
{
    static bool firstEvent = true;

    if (event.Is<WndKeyEvent>()) {
        const WndKeyEvent& keyEvent = event.Get<WndKeyEvent>();

        if (keyEvent.IsPressed() || keyEvent.IsHold()) {
            const float finalSpeed = CAMERA_SPEED;

            if (keyEvent.key == WndKey::KEY_W) { 
                camera.velocity.z = -finalSpeed;
            }
            if (keyEvent.key == WndKey::KEY_S) {
                camera.velocity.z = finalSpeed;
            }
            if (keyEvent.key == WndKey::KEY_A) {
                camera.velocity.x = -finalSpeed;
            }
            if (keyEvent.key == WndKey::KEY_D) {
                camera.velocity.x = finalSpeed;
            }
            if (keyEvent.key == WndKey::KEY_E) {
                camera.velocity.y = finalSpeed;
            }
            if (keyEvent.key == WndKey::KEY_Q) {
                camera.velocity.y = -finalSpeed;
            }
            if (keyEvent.key == WndKey::KEY_F5) {
                firstEvent = true;
            }
        }

        if (keyEvent.IsReleased()) {
            if (keyEvent.key == WndKey::KEY_W) {
                camera.velocity.z = 0;
            }
            if (keyEvent.key == WndKey::KEY_S) {
                camera.velocity.z = 0;
            }
            if (keyEvent.key == WndKey::KEY_A) {
                camera.velocity.x = 0;
            }
            if (keyEvent.key == WndKey::KEY_D) {
                camera.velocity.x = 0;
            }
            if (keyEvent.key == WndKey::KEY_E) {
                camera.velocity.y = 0;
            }
            if (keyEvent.key == WndKey::KEY_Q) {
                camera.velocity.y = 0;
            }
        }
    }

    if (event.Is<WndCursorEvent>()) {
        const WndCursorEvent& cursorEvent = event.Get<WndCursorEvent>();

        static int16_t prevX = 0;
        static int16_t prevY = 0; 

        if (firstEvent) {
            firstEvent = false;
        } else {
            camera.yaw += (float)(cursorEvent.x - prevX) / 200.f;
            camera.pitch -= (float)(cursorEvent.y - prevY) / 200.f;
        }

        prevX = cursorEvent.x;
        prevY = cursorEvent.y;
    }
}


void ProcessWndEvents(const WndEvent& event)
{
    if (event.Is<WndResizeEvent>()) {
        s_swapchainRecreateRequired = true;
    }

    if (event.Is<WndKeyEvent>()) {
        const WndKeyEvent& keyEvent = event.Get<WndKeyEvent>();

        if (keyEvent.key == WndKey::KEY_F5 && keyEvent.IsPressed()) {
            s_flyCameraMode = !s_flyCameraMode;
            
            ShowCursor(!s_flyCameraMode);
        }

        if (keyEvent.IsPressed() || keyEvent.IsHold()) {
            if (keyEvent.key == WndKey::KEY_LEFT) {
                s_dbgTexIdx = s_dbgTexIdx >= 1 ? s_dbgTexIdx - 1 : 0;
            } else if (keyEvent.key == WndKey::KEY_RIGHT) {
                s_dbgTexIdx = glm::clamp<uint32_t>(s_dbgTexIdx + 1, 0, _countof(DBG_TEX_OUTPUT_NAMES) - 1);
            }
        }
    }

    if (s_flyCameraMode) {
        CameraProcessWndEvent(s_camera, event);
    }
}


void ProcessFrame()
{
    ENG_PROFILE_BEGIN_FRAME("Frame");

    Timer timer;

    s_pWnd->ProcessEvents();
    
    WndEvent event;
    while(s_pWnd->PopEvent(event)) {
        ProcessWndEvents(event);
    }

    if (s_pWnd->IsMinimized()) {
        return;
    }

    if (s_swapchainRecreateRequired) {
        if (ResizeVkSwapchain(s_pWnd)) {
            return;
        }

        s_vkDevice.WaitIdle();
        CreateDepthImage();
    }

    s_camera.Update();

    RenderScene();

    UpdateTimings(s_pWnd, timer);

    ++s_frameNumber;

    ENG_PROFILE_END_FRAME("Frame");
}


int main(int argc, char* argv[])
{
    s_camera.velocity = glm::vec3(0.f);
	s_camera.position = glm::vec3(0.f, 2.f, 0.f);
    s_camera.pitch = 0.f;
    s_camera.yaw = glm::radians(90.f);

    wndSysInit();
    s_pWnd = wndSysGetMainWindow();

    WindowInitInfo wndInitInfo = {};
    wndInitInfo.pTitle = APP_NAME;
    wndInitInfo.width = 980;
    wndInitInfo.height = 640;
    wndInitInfo.isVisible = false;

    s_pWnd->Create(wndInitInfo);
    ENG_ASSERT(s_pWnd->IsInitialized());

    CreateVkInstance();    

    vkn::SurfaceCreateInfo vkSurfCreateInfo = {};
    vkSurfCreateInfo.pInstance = &s_vkInstance;
    vkSurfCreateInfo.pWndHandle = s_pWnd->GetNativeHandle();

    s_vkSurface.Create(vkSurfCreateInfo);
    CORE_ASSERT(s_vkSurface.IsCreated());

    CreateVkPhysAndLogicalDevices();

#ifdef ENG_PROFILING_ENABLED
    vkn::GetProfiler().Create(&s_vkDevice);
    CORE_ASSERT(vkn::GetProfiler().IsCreated());
#endif

    CreateVkSwapchain();

    vkn::CmdPoolCreateInfo vkCmdPoolCreateInfo = {};
    vkCmdPoolCreateInfo.pDevice = &s_vkDevice;
    vkCmdPoolCreateInfo.queueFamilyIndex = s_vkDevice.GetQueueFamilyIndex();
    vkCmdPoolCreateInfo.flags =  VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    s_vkCmdPool.Create(vkCmdPoolCreateInfo);
    CORE_ASSERT(s_vkCmdPool.IsCreated());
    s_vkCmdPool.SetDebugName("COMMON_CMD_POOL");
    
    s_vkImmediateSubmitCmdBuffer = s_vkCmdPool.AllocCmdBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    CORE_ASSERT(s_vkImmediateSubmitCmdBuffer.IsCreated());
    s_vkImmediateSubmitCmdBuffer.SetDebugName("IMMEDIATE_CMD_BUFFER");

    s_vkImmediateSubmitFinishedFence.Create(&s_vkDevice);

    vkn::QueryCreateInfo queryCreateInfo = {};
    queryCreateInfo.pDevice = &s_vkDevice;
    queryCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryCreateInfo.queryCount = 128;

    s_vkQueryPool.Create(queryCreateInfo);
    CORE_ASSERT(s_vkQueryPool.IsCreated());
    s_vkQueryPool.SetDebugName("COMMON_GPU_QUERY_POOL");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        cmdBuffer.CmdResetQueryPool(s_vkQueryPool);
    });

    s_vkDescriptorPool = CreateVkDescriptorPool(s_vkDevice.Get());
    s_vkDescriptorSetLayout = CreateVkDescriptorSetLayout(s_vkDevice.Get());
    s_vkDescriptorSet = CreateVkDescriptorSet(s_vkDevice.Get(), s_vkDescriptorPool, s_vkDescriptorSetLayout);

    s_vkPipelineLayout = CreateVkPipelineLayout(s_vkDevice.Get(), s_vkDescriptorSetLayout);
    s_vkPipeline = CreateVkGraphicsPipeline(s_vkDevice.Get(), s_vkPipelineLayout, "shaders/bin/test.vs.spv", "shaders/bin/test.ps.spv");

    const size_t swapchainImageCount = s_vkSwapchain.GetImageCount();

    s_vkRenderingFinishedSemaphores.resize(swapchainImageCount);
    for (size_t i = 0; i < swapchainImageCount; ++i) {
        s_vkRenderingFinishedSemaphores[i].Create(&s_vkDevice);
        CORE_ASSERT(s_vkRenderingFinishedSemaphores[i].IsCreated());

        s_vkRenderingFinishedSemaphores[i].SetDebugName("RND_FINISH_SEMAPHORE_%zu", i);
    }

    s_vkPresentFinishedSemaphore.Create(&s_vkDevice);
    CORE_ASSERT(s_vkPresentFinishedSemaphore.IsCreated());
    s_vkPresentFinishedSemaphore.SetDebugName("PRESENT_FINISH_SEMAPHORE");

    s_vkRenderingFinishedFence.Create(&s_vkDevice);
    CORE_ASSERT(s_vkRenderingFinishedFence.IsCreated());
    s_vkRenderingFinishedFence.SetDebugName("RND_FINISH_FENCE");
    
    s_vkRenderCmdBuffer = s_vkCmdPool.AllocCmdBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    CORE_ASSERT(s_vkRenderCmdBuffer.IsCreated());
    s_vkRenderCmdBuffer.SetDebugName("RND_CMD_BUFFER");

    CreateDepthImage();
    CreateCommonSamplers();

    LoadScene(argc > 1 ? argv[1] : "../assets/Sponza/Sponza.gltf");

    WriteDescriptorSet();

    s_pWnd->SetVisible(true);

    while(!s_pWnd->IsClosed()) {
        ProcessFrame();
    }

    s_vkDevice.WaitIdle();

    vkDestroyPipeline(s_vkDevice.Get(), s_vkPipeline, nullptr);
    vkDestroyPipelineLayout(s_vkDevice.Get(), s_vkPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_vkDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(s_vkDevice.Get(), s_vkDescriptorPool, nullptr);

    s_pWnd->Destroy();
    wndSysTerminate();

    return 0;
}