#include "core/engine/wnd_system/wnd_system.h"

#ifdef ENG_OS_WINDOWS
    #include "core/platform/native/win32/window/win32_window.h"
#endif


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

#include "render/core/vulkan/vk_memory.h"

#include "core/engine/camera/camera.h"

#include "core/profiler/cpu_profiler.h"
#include "render/core/vulkan/vk_profiler.h"

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_win32.h>

#define USE_FASTGLTF

#ifdef USE_FASTGLTF
#define FREEIMAGE_LIB
#include <FreeImage.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>

namespace gltf = fastgltf;
#else
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#include <tiny_gltf.h>

namespace gltf = tinygltf;
#endif


namespace fs = std::filesystem;
using IndexType = uint32_t;


// GPU structures

struct BASE_BINDLESS_REGISTRY
{
    glm::vec3 PAD0;
    glm::uint INST_INFO_IDX;
};


struct BASE_CULLING_BINDLESS_REGISTRY
{
    glm::vec3 PAD0;
    glm::uint INST_COUNT;
};


struct COMMON_TRANSFORM
{
    glm::vec4 MATR[3];
};


struct COMMON_MATERIAL
{
    int32_t ALBEDO_TEX_IDX;
    int32_t NORMAL_TEX_IDX;
    int32_t MR_TEX_IDX;
    int32_t AO_TEX_IDX;
    int32_t EMISSIVE_TEX_IDX;
};


struct COMMON_MESH_INFO
{
    glm::uint FIRST_VERTEX;
    glm::uint VERTEX_COUNT;
    glm::uint FIRST_INDEX;
    glm::uint INDEX_COUNT;

    glm::vec3 BOUNDS_MIN_LCS;
    glm::uint PAD0;
    glm::vec3 BOUNDS_MAX_LCS;
    glm::uint PAD1;
};


struct COMMON_INST_INFO
{
    glm::uint TRANSFORM_IDX;
    glm::uint MATERIAL_IDX;
    glm::uint MESH_IDX;
    glm::uint PAD0;
};


struct BASE_INDIRECT_DRAW_CMD
{
    // NOTE: Don't change order of this variables!!!
    glm::uint INDEX_COUNT;
    glm::uint INSTANCE_COUNT;
    glm::uint FIRST_INDEX;
    int32_t   VERTEX_OFFSET;
    glm::uint FIRST_INSTANCE;

    glm::uint INSTANCE_INFO_IDX;
};


struct FRUSTUM_PLANE
{
    glm::vec3 normal;
    float distance;
};


static constexpr glm::uint COMMON_FRUSTUM_PLANES_COUNT = 6;


struct FRUSTUM
{
    FRUSTUM_PLANE planes[COMMON_FRUSTUM_PLANES_COUNT];
};


static_assert(sizeof(FRUSTUM) == sizeof(math::Frustum));


struct COMMON_CB_DATA
{
    glm::mat4x4 COMMON_VIEW_MATRIX;
    glm::mat4x4 COMMON_PROJ_MATRIX;
    glm::mat4x4 COMMON_VIEW_PROJ_MATRIX;

    FRUSTUM COMMON_CAMERA_FRUSTUM;

    glm::uint  COMMON_FLAGS;
    glm::uint  COMMON_DBG_FLAGS;
    glm::uvec2 PAD0;
};


enum class COMMON_DBG_FLAG_MASKS
{
    OUTPUT_COMMON_MTL_ALBEDO_TEX = 0x1,
    OUTPUT_COMMON_MTL_NORMAL_TEX = 0x2,
    OUTPUT_COMMON_MTL_MR_TEX = 0x4,
    OUTPUT_COMMON_MTL_AO_TEX = 0x8,
    OUTPUT_COMMON_MTL_EMISSIVE_TEX = 0x10,

    USE_MESH_INDIRECT_DRAW = 0x20,
    USE_MESH_GPU_CULLING = 0x40
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


// CPU structures

struct TextureLoadData
{
    ~TextureLoadData()
    {
        if (pBitmap) {
            FreeImage_Unload(pBitmap);
            pBitmap = nullptr;
        }

    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        name.fill('\0');
    #endif
        format = VK_FORMAT_UNDEFINED;
    }

    uint32_t GetWidth() const { return FreeImage_GetWidth(pBitmap); }
    uint32_t GetHeight() const { return FreeImage_GetHeight(pBitmap); }
    uint32_t GetBitsPerPixel() const { return FreeImage_GetBPP(pBitmap); }
    
    uint32_t GetTexSize() const { return GetWidth() * GetHeight() * (GetBitsPerPixel() / 8); }

    const void* GetData() const { return FreeImage_GetBits(pBitmap); } 

#ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
    std::array<char, vkn::Object::MAX_OBJ_DBG_NAME_LENGTH> name = {};
#endif
    FIBITMAP* pBitmap = nullptr;
    VkFormat format;
};


struct Material
{
    int32_t albedoTexIdx     = -1;
    int32_t normalTexIdx     = -1;
    int32_t metalRoughTexIdx = -1;
    int32_t aoTexIdx         = -1;
    int32_t emissiveTexIdx   = -1;
};


static constexpr uint32_t VERTEX_DATA_SIZE_UI4 = 1;

struct Vertex
{
    void Pack(const glm::vec3& lpos, const glm::vec3& lnorm, glm::vec2 uv)
    {
        data[0].x = glm::packHalf2x16(glm::vec2(lpos.x, lpos.y));
        data[0].y = glm::packHalf2x16(glm::vec2(lpos.z, lnorm.x));
        data[0].z = glm::packHalf2x16(glm::vec2(lnorm.y, lnorm.z));
        data[0].w = glm::packHalf2x16(uv);
    }

    void Unpack(glm::vec3& outLPos, glm::vec3& outLNorm, glm::vec2& outUv)
    {
        outLPos = GetLPos();
        outLNorm = GetLNorm();
        outUv = GetUV();
    }

    glm::vec3 GetLPos() const
    {
        return glm::vec3(glm::unpackHalf2x16(data[0].x), glm::unpackHalf2x16(data[0].y).x);
    }

    glm::vec3 GetLNorm() const
    {
        return glm::vec3(glm::unpackHalf2x16(data[0].y).y, glm::unpackHalf2x16(data[0].z));
    }

    glm::vec2 GetUV() const
    {
        return glm::unpackHalf2x16(data[0].w);
    }

    glm::u32vec4 data[VERTEX_DATA_SIZE_UI4] = {};
};


struct AABB
{
    glm::vec3 min;
    glm::vec3 max;
};


struct Mesh
{
    uint32_t firstVertex;
    uint32_t vertexCount;
    uint32_t firstIndex;
    uint32_t indexCount;

    AABB bounds;
};


struct DIP
{
    uint32_t meshIdx;
    int32_t mtlIdx = -1;
};


struct DIPGroup
{
    uint32_t firstDIP;
    uint32_t dipCount;
};


struct SceneNode
{
    std::vector<uint32_t> childrenNodes;
    std::string name;

    int32_t trsIdx;
    int32_t dipGroupIdx = -1;
};


static constexpr size_t COMMON_SAMPLERS_DESCRIPTOR_SLOT = 0;
static constexpr size_t COMMON_CONST_BUFFER_DESCRIPTOR_SLOT = 1;
static constexpr size_t COMMON_MESH_INFOS_DESCRIPTOR_SLOT = 2;
static constexpr size_t COMMON_TRANSFORMS_DESCRIPTOR_SLOT = 3;
static constexpr size_t COMMON_MATERIALS_DESCRIPTOR_SLOT = 4;
static constexpr size_t COMMON_MTL_TEXTURES_DESCRIPTOR_SLOT = 5;
static constexpr size_t COMMON_INST_INFOS_DESCRIPTOR_SLOT = 6;
static constexpr size_t COMMON_VERTEX_DATA_DESCRIPTOR_SLOT = 7;
static constexpr size_t BASE_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT = 8;
static constexpr size_t BASE_INDIRECT_DRAW_CMDS_COUNT_DESCRIPTOR_SLOT = 9;

static constexpr uint32_t COMMON_MTL_TEXTURES_COUNT = 128;

static constexpr uint32_t MAX_INDIRECT_DRAW_CMD_COUNT = 1024;

static constexpr const char* APP_NAME = "Vulkan Demo";

static constexpr bool VSYNC_ENABLED = false;

static constexpr float CAMERA_SPEED = 0.0025f;

static constexpr size_t TEXTURE_RGBA8_MAX_SIZE = 4096 * 4096 * 4 * sizeof(uint32_t);
static constexpr size_t STAGING_BUFFER_SIZE = TEXTURE_RGBA8_MAX_SIZE;
static constexpr size_t STAGING_BUFFER_COUNT = 2;


static Window* s_pWnd = nullptr;

static vkn::Instance& s_vkInstance = vkn::GetInstance();
static vkn::Surface& s_vkSurface = vkn::GetSurface();

static vkn::PhysicalDevice& s_vkPhysDevice = vkn::GetPhysicalDevice();
static vkn::Device&         s_vkDevice = vkn::GetDevice();

static vkn::Allocator& s_vkAllocator = vkn::GetAllocator();

static vkn::QueryPool s_queryPool;

static vkn::Swapchain& s_vkSwapchain = vkn::GetSwapchain();
static vkn::Image      s_depthRT;
static vkn::ImageView  s_depthRTView;

static vkn::CmdPool   s_cmdPool;
static vkn::CmdBuffer s_immediateSubmitCmdBuffer;

static vkn::Fence s_immediateSubmitFinishedFence;

static std::array<vkn::Buffer, STAGING_BUFFER_COUNT> s_commonStagingBuffers;

static std::vector<vkn::Semaphore> s_renderFinishedSemaphores;
static vkn::Semaphore              s_presentFinishedSemaphore;
static vkn::Fence                  s_renderFinishedFence;
static vkn::CmdBuffer              s_renderCmdBuffer;

static VkDescriptorPool      s_commonDescriptorPool = VK_NULL_HANDLE;
static VkDescriptorSet       s_commonDescriptorSet = VK_NULL_HANDLE;
static VkDescriptorSetLayout s_commonDescriptorSetLayout = VK_NULL_HANDLE;

static VkPipelineLayout s_basePipelineLayout = VK_NULL_HANDLE;
static VkPipeline       s_basePipeline = VK_NULL_HANDLE;

static VkPipelineLayout s_baseCullingPipelineLayout = VK_NULL_HANDLE;
static VkPipeline       s_baseCullingPipeline = VK_NULL_HANDLE;

static vkn::Buffer s_vertexBuffer;
static vkn::Buffer s_indexBuffer;

static vkn::Buffer s_commonConstBuffer;

static vkn::Buffer s_commonMeshDataBuffer;
static vkn::Buffer s_commonMaterialsBuffer;
static vkn::Buffer s_commonTransformsBuffer;
static vkn::Buffer s_commonInstDataBuffer;

static vkn::Buffer s_drawIndirectCommandsBuffer;
static vkn::Buffer s_drawIndirectCommandsCountBuffer;

static std::vector<vkn::Image>     s_sceneImages;
static std::vector<vkn::ImageView> s_sceneImageViews;
static std::vector<vkn::Sampler>   s_commonSamplers;

static vkn::Image     s_sceneDefaultImage;
static vkn::ImageView s_sceneDefaultImageView;


static std::vector<glm::mat3x4> s_cpuTransforms;

static std::vector<Vertex> s_cpuVertexBuffer;
static std::vector<IndexType> s_cpuIndexBuffer;

static std::vector<Mesh> s_cpuMeshes;
static std::vector<DIP> s_cpuDIPs;
static std::vector<DIPGroup> s_cpuDIPGroups;

static std::vector<Material> s_cpuMaterials;
static std::vector<TextureLoadData> s_cpuTexturesData;

static std::vector<SceneNode> s_cpuSceneNodes;


static eng::Camera s_camera;
static glm::vec3 s_cameraVel = M3D_ZEROF3;

static uint32_t s_dbgTexIdx = 0;

static size_t s_frameNumber = 0;
static float s_frameTime = 0.f;
static bool s_swapchainRecreateRequired = false;
static bool s_flyCameraMode = false;

#ifndef ENG_BUILD_RELEASE
static bool s_useMeshIndirectDraw = true;
static bool s_useMeshCulling = true;

// Uses for debug purposes during CPU frustum culling
static size_t s_dbgDrawnMeshCount = 0;
#endif


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


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


namespace DbgUI
{
    static void Init()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; 

    #ifdef ENG_OS_WINDOWS
        if (!ImGui_ImplWin32_Init(s_pWnd->GetNativeHandle())) {
            CORE_ASSERT_FAIL("Failed to initialize ImGui Win32 part");
        }

        ImGui::GetPlatformIO().Platform_CreateVkSurface = [](ImGuiViewport* viewport, ImU64 vkInstance, const void* vkAllocator, ImU64* outVkSurface)
        {
            VkWin32SurfaceCreateInfoKHR createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            createInfo.hwnd = (HWND)viewport->PlatformHandleRaw;
            createInfo.hinstance = ::GetModuleHandle(nullptr);
            return (int)vkCreateWin32SurfaceKHR((VkInstance)vkInstance, &createInfo, (VkAllocationCallbacks*)vkAllocator, (VkSurfaceKHR*)outVkSurface);
        };
    #endif

        ImGui_ImplVulkan_InitInfo imGuiVkInitInfo = {};
        imGuiVkInitInfo.ApiVersion = s_vkInstance.GetApiVersion();
        imGuiVkInitInfo.Instance = s_vkInstance.Get();
        imGuiVkInitInfo.PhysicalDevice = s_vkPhysDevice.Get();
        imGuiVkInitInfo.Device = s_vkDevice.Get();
        imGuiVkInitInfo.QueueFamily = s_vkDevice.GetQueueFamilyIndex();
        imGuiVkInitInfo.Queue = s_vkDevice.GetQueue();
        imGuiVkInitInfo.DescriptorPoolSize = 1000;
        imGuiVkInitInfo.MinImageCount = 2;
        imGuiVkInitInfo.ImageCount = s_vkSwapchain.GetImageCount();
        imGuiVkInitInfo.PipelineCache = VK_NULL_HANDLE;

        imGuiVkInitInfo.UseDynamicRendering = true;
        imGuiVkInitInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT; // 0 defaults to VK_SAMPLE_COUNT_1_BIT
    #ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
        imGuiVkInitInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        imGuiVkInitInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        imGuiVkInitInfo.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
        
        const VkFormat swapchainFormat = s_vkSwapchain.GetImageFormat();
        imGuiVkInitInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainFormat;
    #else
        #error Vulkan Dynamic Rendering Is Not Supported. Get Vulkan SDK Latests.
    #endif
        imGuiVkInitInfo.CheckVkResultFn = [](VkResult error) { VK_CHECK(error); };
        imGuiVkInitInfo.MinAllocationSize = 1024 * 1024;

        if (!ImGui_ImplVulkan_Init(&imGuiVkInitInfo)) {
            CORE_ASSERT_FAIL("Failed to initialize ImGui Vulkan part");
        }

    #ifdef ENG_OS_WINDOWS
        dynamic_cast<Win32Window*>(s_pWnd)->AddEventCallback([](HWND wHwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) -> LRESULT
        {
            return ::ImGui_ImplWin32_WndProcHandler(wHwnd, uMsg, wParam, lParam);
        });
    #endif
    }


    static void Terminate()
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }


    static bool IsAnyWindowFocused()
    {
        return ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);
    }


    static void BeginFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }


    static void EndFrame()
    {
        ImGui::EndFrame();
    }


    static void FillData()
    {
        if (ImGui::Begin("Settings")) {
        #if defined(ENG_BUILD_DEBUG)
            constexpr const char* BUILD_TYPE_STR = "DEBUG";
        #elif defined(ENG_BUILD_PROFILE)
            constexpr const char* BUILD_TYPE_STR = "PROFILE";
        #else
            constexpr const char* BUILD_TYPE_STR = "RELEASE";
        #endif            

            ImGui::Text("Build Type: %s", BUILD_TYPE_STR);
            ImGui::Text("CPU: %.3f ms (%.1f FPS)", s_frameTime, 1000.f / s_frameTime);

            ImGui::Separator();
            ImGui::NewLine();

            ImGui::Text("Cursor Position: [x: %d, y: %d]", s_pWnd->GetCursorX(), s_pWnd->GetCursorY());
            ImGui::Text("Fly Camera Mode (F5):");
            ImGui::SameLine(); 
            ImGui::TextColored(ImVec4(!s_flyCameraMode, s_flyCameraMode, 0.f, 1.f), s_flyCameraMode ? "ON" : "OFF");
            
            ImGui::Separator();
            ImGui::NewLine();

            ImGui::Text("Material Debug Texture: %s", DBG_TEX_OUTPUT_NAMES[s_dbgTexIdx]);
            if (ImGui::IsItemHovered()) {
                if (ImGui::BeginTooltip()) {
                    ImGui::Text("Use <-/-> arrows to switch");
                }
                ImGui::EndTooltip();
            }

        #ifndef ENG_BUILD_RELEASE
            ImGui::Checkbox("BasePass/Use Indirect Draw", &s_useMeshIndirectDraw);
            ImGui::Checkbox("BasePass/Use Culling", &s_useMeshCulling);
            if (!s_useMeshIndirectDraw) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.f, 1.f), "(Drawn Mesh Count: %zu)", s_dbgDrawnMeshCount);
            }
        #endif
        } ImGui::End();
    }


    static void Render(vkn::CmdBuffer& cmdBuffer)
    {   
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Dbg_UI_Render_Pass", 255, 50, 50, 255);

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer.Get());
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
    static_assert(std::is_same_v<IndexType, uint8_t> || std::is_same_v<IndexType, uint16_t> || std::is_same_v<IndexType, uint32_t>);

    if constexpr (std::is_same_v<IndexType, uint8_t>) {
        return VK_INDEX_TYPE_UINT8;
    } else if constexpr (std::is_same_v<IndexType, uint16_t>) {
        return VK_INDEX_TYPE_UINT16;
    } else {
        return VK_INDEX_TYPE_UINT32;
    }
}


template <typename Func, typename... Args>
static void ImmediateSubmitQueue(VkQueue vkQueue, Func func, Args&&... args)
{   
    s_immediateSubmitFinishedFence.Reset();
    s_immediateSubmitCmdBuffer.Reset();

    VkCommandBufferBeginInfo cmdBI = {};
    cmdBI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    s_immediateSubmitCmdBuffer.Begin(cmdBI);
        func(s_immediateSubmitCmdBuffer, std::forward<Args>(args)...);
    s_immediateSubmitCmdBuffer.End();

    SubmitVkQueue(
        vkQueue, 
        s_immediateSubmitCmdBuffer.Get(), 
        s_immediateSubmitFinishedFence.Get(), 
        VK_NULL_HANDLE, 
        VK_PIPELINE_STAGE_2_NONE,
        VK_NULL_HANDLE, 
        VK_PIPELINE_STAGE_2_NONE
    );

    s_immediateSubmitFinishedFence.WaitFor(10'000'000'000);
}


static void CreateVkInstance()
{
#ifdef ENG_VK_DEBUG_UTILS_ENABLED
    vkn::InstanceDebugMessengerCreateInfo vkDbgMessengerCI = {};
    vkDbgMessengerCI.messageType = 
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    vkDbgMessengerCI.messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    vkDbgMessengerCI.pMessageCallback = DbgVkMessageCallback;

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

    vkn::InstanceCreateInfo vkInstCI = {};
    vkInstCI.pApplicationName = APP_NAME;
    vkInstCI.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    vkInstCI.pEngineName = "VkEngine";
    vkInstCI.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    vkInstCI.apiVersion = VK_API_VERSION_1_3;
    vkInstCI.extensions = vkInstExtensions;
#ifdef ENG_VK_DEBUG_UTILS_ENABLED
    vkInstCI.layers = vkInstLayers;
    vkInstCI.pDbgMessengerCreateInfo = &vkDbgMessengerCI;
#endif

    s_vkInstance.Create(vkInstCI); 
    CORE_ASSERT(s_vkInstance.IsCreated());
}


static void CreateVkSwapchain()
{
    vkn::SwapchainCreateInfo vkSwapchainCI = {};
    vkSwapchainCI.pDevice = &s_vkDevice;
    vkSwapchainCI.pSurface = &s_vkSurface;

    vkSwapchainCI.width = s_pWnd->GetWidth();
    vkSwapchainCI.height = s_pWnd->GetHeight();

    vkSwapchainCI.minImageCount    = 2;
    vkSwapchainCI.imageFormat      = VK_FORMAT_B8G8R8A8_SRGB;
    vkSwapchainCI.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    vkSwapchainCI.imageArrayLayers = 1u;
    vkSwapchainCI.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    vkSwapchainCI.transform        = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    vkSwapchainCI.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    vkSwapchainCI.presentMode      = VSYNC_ENABLED ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

    s_vkSwapchain.Create(vkSwapchainCI);
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
    vkPhysDeviceFeturesReq.vertexPipelineStoresAndAtomics = true;

    vkn::PhysicalDevicePropertiesRequirenments vkPhysDevicePropsReq = {};
    vkPhysDevicePropsReq.deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

    vkn::PhysicalDeviceCreateInfo vkPhysDeviceCI = {};
    vkPhysDeviceCI.pInstance = &s_vkInstance;
    vkPhysDeviceCI.pPropertiesRequirenments = &vkPhysDevicePropsReq;
    vkPhysDeviceCI.pFeaturesRequirenments = &vkPhysDeviceFeturesReq;

    s_vkPhysDevice.Create(vkPhysDeviceCI);
    CORE_ASSERT(s_vkPhysDevice.IsCreated()); 


    constexpr std::array vkDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12 = {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = &features13;
    features12.bufferDeviceAddress = VK_TRUE;
    features12.bufferDeviceAddressCaptureReplay = VK_TRUE;
    features12.descriptorBindingPartiallyBound = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;
    features12.samplerMirrorClampToEdge = VK_TRUE;
    features12.drawIndirectCount = VK_TRUE;

    VK_ASSERT(s_vkPhysDevice.GetFeatures11().shaderDrawParameters);

    VkPhysicalDeviceVulkan11Features features11 = {};
    features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features11.pNext = &features12;
    features11.shaderDrawParameters = VK_TRUE; // Enables slang internal shader variables like "SV_VertexID" etc.

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &features11;
    features2.features.samplerAnisotropy = VK_TRUE;
    features2.features.vertexPipelineStoresAndAtomics = VK_TRUE;

    vkn::DeviceCreateInfo vkDeviceCI = {};
    vkDeviceCI.pPhysDevice = &s_vkPhysDevice;
    vkDeviceCI.pSurface = &s_vkSurface;
    vkDeviceCI.queuePriority = 1.f;
    vkDeviceCI.extensions = vkDeviceExtensions;
    vkDeviceCI.pFeatures2 = &features2;

    s_vkDevice.Create(vkDeviceCI);
    CORE_ASSERT(s_vkDevice.IsCreated());
}


static void CreateCommonStagingBuffers()
{
    vkn::AllocationInfo stagingBufAllocInfo = {};
    stagingBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    stagingBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

    vkn::BufferCreateInfo stagingBufCreateInfo = {};
    stagingBufCreateInfo.pDevice = &s_vkDevice;
    stagingBufCreateInfo.size = STAGING_BUFFER_SIZE;
    stagingBufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufCreateInfo.pAllocInfo = &stagingBufAllocInfo;

    for (size_t i = 0; i < s_commonStagingBuffers.size(); ++i) {
        vkn::Buffer& buffer = s_commonStagingBuffers[i];
        
        buffer.Create(stagingBufCreateInfo);
        CORE_ASSERT_MSG(buffer.IsCreated(), "Failed to create staging buffer %zu", i);
        buffer.SetDebugName("STAGING_BUFFER_%zu", i);
    }
}


static VkShaderModule CreateVkShaderModule(VkDevice vkDevice, const fs::path& shaderSpirVPath, std::vector<uint8_t>* pExternalBuffer = nullptr)
{
    std::vector<uint8_t>* pShaderData = nullptr;
    std::vector<uint8_t> localBuffer;
    
    pShaderData = pExternalBuffer ? pExternalBuffer : &localBuffer;

    const std::string strPath = shaderSpirVPath.string();

    if (!ReadFile(*pShaderData, shaderSpirVPath)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", strPath.c_str());
    }
    VK_ASSERT_MSG(pShaderData->size() % sizeof(uint32_t) == 0, "Size of SPIR-V byte code of %s must be multiple of %zu", strPath.c_str(), sizeof(uint32_t));

    VkShaderModuleCreateInfo shaderModuleCI = {};
    shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCI.pCode = reinterpret_cast<const uint32_t*>(pShaderData->data());
    shaderModuleCI.codeSize = pShaderData->size();

    VkShaderModule vkShaderModule = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(vkDevice, &shaderModuleCI, nullptr, &vkShaderModule));
    VK_ASSERT(vkShaderModule != VK_NULL_HANDLE);

    return vkShaderModule;
}


static VkDescriptorPool CreateVkCommonDescriptorPool(VkDevice vkDevice)
{
    vkn::DescriptorPoolBuilder builder;

    builder
        // .SetFlags(VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT)
        .SetMaxDescriptorSetsCount(1);
        
    builder
        .AddResource(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
        .AddResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7)
        .AddResource(VK_DESCRIPTOR_TYPE_SAMPLER, (uint32_t)COMMON_SAMPLER_IDX::COUNT)
        .AddResource(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, COMMON_MTL_TEXTURES_COUNT);
    
    VkDescriptorPool vkPool = builder.Build(vkDevice);

    return vkPool;
}


static VkDescriptorSetLayout CreateVkCommonDescriptorSetLayout(VkDevice vkDevice)
{
    vkn::DescriptorSetLayoutBuilder builder;

    builder
        // .SetFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT)
        .AddBinding(COMMON_SAMPLERS_DESCRIPTOR_SLOT,     VK_DESCRIPTOR_TYPE_SAMPLER, (uint32_t)COMMON_SAMPLER_IDX::COUNT, VK_SHADER_STAGE_ALL)
        .AddBinding(COMMON_CONST_BUFFER_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL)
        .AddBinding(COMMON_MESH_INFOS_DESCRIPTOR_SLOT,   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL)
        .AddBinding(COMMON_TRANSFORMS_DESCRIPTOR_SLOT,   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL)
        .AddBinding(COMMON_MATERIALS_DESCRIPTOR_SLOT,    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL)
        .AddBinding(COMMON_MTL_TEXTURES_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, COMMON_MTL_TEXTURES_COUNT, VK_SHADER_STAGE_FRAGMENT_BIT)
        .AddBinding(COMMON_INST_INFOS_DESCRIPTOR_SLOT,   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL)
        .AddBinding(COMMON_VERTEX_DATA_DESCRIPTOR_SLOT,   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
        .AddBinding(BASE_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL)
        .AddBinding(BASE_INDIRECT_DRAW_CMDS_COUNT_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL);

    VkDescriptorSetLayout vkLayout = builder.Build(vkDevice);

    return vkLayout;
}


static VkDescriptorSet CreateVkCommonDescriptorSet(VkDevice vkDevice, VkDescriptorPool vkDescriptorPool, VkDescriptorSetLayout vkDescriptorSetLayout)
{
    vkn::DescriptorSetAllocator allocator;

    VkDescriptorSet vkDescriptorSets[] = { VK_NULL_HANDLE };

    allocator
        .SetPool(vkDescriptorPool)
        .AddLayout(vkDescriptorSetLayout)
        .Allocate(vkDevice, vkDescriptorSets);

    return vkDescriptorSets[0];
}


static VkPipelineLayout CreateVkBasePipelineLayout(VkDevice vkDevice, VkDescriptorSetLayout vkDescriptorSetLayout)
{
    vkn::PipelineLayoutBuilder plBuilder(s_vkPhysDevice.GetProperties().limits.maxPushConstantsSize);

    VkPipelineLayout vkLayout = plBuilder
        .AddPushConstantRange(VK_SHADER_STAGE_ALL, 0, sizeof(BASE_BINDLESS_REGISTRY))
        .AddDescriptorSetLayout(vkDescriptorSetLayout)
        .Build(vkDevice);

    return vkLayout;
}


static VkPipelineLayout CreateVkBaseCullingPipelineLayout(VkDevice vkDevice, VkDescriptorSetLayout vkDescriptorSetLayout)
{
    vkn::PipelineLayoutBuilder plBuilder(s_vkPhysDevice.GetProperties().limits.maxPushConstantsSize);

    VkPipelineLayout vkLayout = plBuilder
        .AddPushConstantRange(VK_SHADER_STAGE_ALL, 0, sizeof(BASE_CULLING_BINDLESS_REGISTRY))
        .AddDescriptorSetLayout(vkDescriptorSetLayout)
        .Build(vkDevice);

    return vkLayout;
}


static VkPipeline CreateVkBasePipeline(VkDevice vkDevice, VkPipelineLayout vkLayout, const fs::path& vsPath, const fs::path& psPath)
{
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

    return vkPipeline;
}


static VkPipeline CreateVkBaseCullingPipeline(VkDevice vkDevice, VkPipelineLayout vkLayout, const fs::path& csPath)
{
    std::vector<uint8_t> shaderCodeBuffer;
    VkShaderModule vkShaderModule = CreateVkShaderModule(vkDevice, csPath, &shaderCodeBuffer);

    vkn::ComputePipelineBuilder builder;

    VkPipeline vkPipeline = builder
        .SetShader(vkShaderModule, "main")
        .SetLayout(vkLayout)
        .Build(vkDevice);
    
    vkDestroyShaderModule(vkDevice, vkShaderModule, nullptr);
    vkShaderModule = VK_NULL_HANDLE;

    return vkPipeline;
}


void CreateVkIndirectDrawBuffers()
{
    vkn::AllocationInfo ai = {};
    ai.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    ai.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo ci = {};
    ci.pDevice = &s_vkDevice;
    ci.size = MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(BASE_INDIRECT_DRAW_CMD);
    ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    ci.pAllocInfo = &ai;

    s_drawIndirectCommandsBuffer.Create(ci); 
    CORE_ASSERT(s_drawIndirectCommandsBuffer.IsCreated());
    s_drawIndirectCommandsBuffer.SetDebugName("DRAW_INDIRECT_COMMAND_BUFFER");

    ci.size = sizeof(glm::uint);

    s_drawIndirectCommandsCountBuffer.Create(ci); 
    CORE_ASSERT(s_drawIndirectCommandsCountBuffer.IsCreated());
    s_drawIndirectCommandsCountBuffer.SetDebugName("DRAW_INDIRECT_COMMAND_COUNT_BUFFER");
}


static void CreateDepthRT()
{
    vkn::Image& depthImage = s_depthRT;
    vkn::ImageView& depthImageView = s_depthRTView;

    if (depthImageView.IsCreated()) {
        depthImageView.Destroy();
    }

    if (depthImage.IsCreated()) {
        depthImage.Destroy();
    }

    vkn::AllocationInfo depthImageAI = {};
    depthImageAI.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    depthImageAI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::ImageCreateInfo depthImageCI = {};
    depthImageCI.pDevice = &s_vkDevice;

    depthImageCI.type = VK_IMAGE_TYPE_2D;
    depthImageCI.extent = VkExtent3D{s_pWnd->GetWidth(), s_pWnd->GetHeight(), 1};
    depthImageCI.format = VK_FORMAT_D32_SFLOAT;
    depthImageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; 
    depthImageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthImageCI.flags = 0;
    depthImageCI.mipLevels = 1;
    depthImageCI.arrayLayers = 1;
    depthImageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageCI.pAllocInfo = &depthImageAI;

    depthImage.Create(depthImageCI);
    CORE_ASSERT(depthImage.IsCreated());
    depthImage.SetDebugName("COMMON_DEPTH");

    vkn::ImageViewCreateInfo depthImageViewCI = {};
    depthImageViewCI.pOwner = &depthImage;
    depthImageViewCI.type = VK_IMAGE_VIEW_TYPE_2D;
    depthImageViewCI.format = VK_FORMAT_D32_SFLOAT;
    depthImageViewCI.components.r = VK_COMPONENT_SWIZZLE_R;
    depthImageViewCI.components.g = VK_COMPONENT_SWIZZLE_G;
    depthImageViewCI.components.b = VK_COMPONENT_SWIZZLE_B;
    depthImageViewCI.components.a = VK_COMPONENT_SWIZZLE_A;
    depthImageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthImageViewCI.subresourceRange.baseMipLevel = 0;
    depthImageViewCI.subresourceRange.levelCount = 1;
    depthImageViewCI.subresourceRange.baseArrayLayer = 0;
    depthImageViewCI.subresourceRange.layerCount = 1;

    depthImageView.Create(depthImageViewCI);
    CORE_ASSERT(depthImageView.IsValid());
    depthImageView.SetDebugName("COMMON_DEPTH_VIEW");
}


static void CreateCommonSamplers()
{
    s_commonSamplers.resize((uint32_t)COMMON_SAMPLER_IDX::COUNT);

    std::vector<vkn::SamplerCreateInfo> smpCIs((uint32_t)COMMON_SAMPLER_IDX::COUNT);

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].pDevice = &s_vkDevice;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].magFilter = VK_FILTER_NEAREST;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].minFilter = VK_FILTER_NEAREST;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].mipLodBias = 0.f;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].anisotropyEnable = VK_FALSE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].compareEnable = VK_FALSE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].minLod = 0.f;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].maxLod = VK_LOD_CLAMP_NONE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].unnormalizedCoordinates = VK_FALSE;

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT].addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT].addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT].addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE].addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE].addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE].addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER].addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER].addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER].addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER].borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE].addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE].addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE].addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT].magFilter = VK_FILTER_LINEAR;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT].minFilter = VK_FILTER_LINEAR;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT].mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT].addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT].addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT].addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE].addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE].addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE].addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER].addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER].addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER].addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER].borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE].addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE].addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE].addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;


    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_REPEAT].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_REPEAT].maxAnisotropy = 2.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRRORED_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRRORED_REPEAT].maxAnisotropy = 2.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_EDGE].maxAnisotropy = 2.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_BORDER] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_BORDER].maxAnisotropy = 2.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRROR_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 2.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_REPEAT].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_REPEAT].maxAnisotropy = 2.f;

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRRORED_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRRORED_REPEAT].maxAnisotropy = 2.f;

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_EDGE].maxAnisotropy = 2.f;

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_BORDER] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_BORDER].maxAnisotropy = 2.f;

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRROR_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 2.f;


    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_REPEAT].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_REPEAT].maxAnisotropy = 4.f;

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRRORED_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRRORED_REPEAT].maxAnisotropy = 4.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_EDGE].maxAnisotropy = 4.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_BORDER] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_BORDER].maxAnisotropy = 4.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRROR_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 4.f;

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_REPEAT].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_REPEAT].maxAnisotropy = 4.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRRORED_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRRORED_REPEAT].maxAnisotropy = 4.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_EDGE].maxAnisotropy = 4.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_BORDER] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_BORDER].maxAnisotropy = 4.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRROR_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 4.f;


    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_REPEAT].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_REPEAT].maxAnisotropy = 8.f;

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRRORED_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRRORED_REPEAT].maxAnisotropy = 8.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_EDGE].maxAnisotropy = 8.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_BORDER] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_BORDER].maxAnisotropy = 8.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRROR_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 8.f;

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_REPEAT].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_REPEAT].maxAnisotropy = 8.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRRORED_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRRORED_REPEAT].maxAnisotropy = 8.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_EDGE].maxAnisotropy = 8.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_BORDER] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_BORDER].maxAnisotropy = 8.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRROR_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 8.f;


    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_REPEAT].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_REPEAT].maxAnisotropy = 16.f;

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRRORED_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRRORED_REPEAT].maxAnisotropy = 16.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_EDGE].maxAnisotropy = 16.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_BORDER] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_BORDER].maxAnisotropy = 16.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRROR_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 16.f;

    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT].maxAnisotropy = 16.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRRORED_REPEAT] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRRORED_REPEAT].maxAnisotropy = 16.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_EDGE].maxAnisotropy = 16.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_BORDER] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_BORDER].maxAnisotropy = 16.f;
    
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRROR_CLAMP_TO_EDGE] = smpCIs[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE];
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    smpCIs[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 16.f;


    for (size_t i = 0; i < smpCIs.size(); ++i) {
        s_commonSamplers[i].Create(smpCIs[i]);
        CORE_ASSERT(s_commonSamplers[i].IsCreated());
        s_commonSamplers[i].SetDebugName(COMMON_SAMPLERS_DBG_NAMES[i]);
    }
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
        commonSamplerWrite.dstSet = s_commonDescriptorSet;
        commonSamplerWrite.dstBinding = COMMON_SAMPLERS_DESCRIPTOR_SLOT;
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
    commonConstBufWrite.dstSet = s_commonDescriptorSet;
    commonConstBufWrite.dstBinding = COMMON_CONST_BUFFER_DESCRIPTOR_SLOT;
    commonConstBufWrite.dstArrayElement = 0;
    commonConstBufWrite.descriptorCount = 1;
    commonConstBufWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    commonConstBufWrite.pBufferInfo = &commonConstBufferInfo;

    descWrites.emplace_back(commonConstBufWrite);


    VkDescriptorBufferInfo commonMeshInfoBufferInfo = {};
    commonMeshInfoBufferInfo.buffer = s_commonMeshDataBuffer.Get();
    commonMeshInfoBufferInfo.offset = 0;
    commonMeshInfoBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet commonMeshInfoBufferWrite = {};
    commonMeshInfoBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    commonMeshInfoBufferWrite.dstSet = s_commonDescriptorSet;
    commonMeshInfoBufferWrite.dstBinding = COMMON_MESH_INFOS_DESCRIPTOR_SLOT;
    commonMeshInfoBufferWrite.dstArrayElement = 0;
    commonMeshInfoBufferWrite.descriptorCount = 1;
    commonMeshInfoBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    commonMeshInfoBufferWrite.pBufferInfo = &commonMeshInfoBufferInfo;

    descWrites.emplace_back(commonMeshInfoBufferWrite);


    VkDescriptorBufferInfo commonTrsBufferInfo = {};
    commonTrsBufferInfo.buffer = s_commonTransformsBuffer.Get();
    commonTrsBufferInfo.offset = 0;
    commonTrsBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet commonTrsBufferWrite = {};
    commonTrsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    commonTrsBufferWrite.dstSet = s_commonDescriptorSet;
    commonTrsBufferWrite.dstBinding = COMMON_TRANSFORMS_DESCRIPTOR_SLOT;
    commonTrsBufferWrite.dstArrayElement = 0;
    commonTrsBufferWrite.descriptorCount = 1;
    commonTrsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    commonTrsBufferWrite.pBufferInfo = &commonTrsBufferInfo;

    descWrites.emplace_back(commonTrsBufferWrite);


    VkDescriptorBufferInfo commonMaterialsBufferInfo = {};
    commonMaterialsBufferInfo.buffer = s_commonMaterialsBuffer.Get();
    commonMaterialsBufferInfo.offset = 0;
    commonMaterialsBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet commonMaterialsBufferWrite = {};
    commonMaterialsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    commonMaterialsBufferWrite.dstSet = s_commonDescriptorSet;
    commonMaterialsBufferWrite.dstBinding = COMMON_MATERIALS_DESCRIPTOR_SLOT;
    commonMaterialsBufferWrite.dstArrayElement = 0;
    commonMaterialsBufferWrite.descriptorCount = 1;
    commonMaterialsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    commonMaterialsBufferWrite.pBufferInfo = &commonMaterialsBufferInfo;

    descWrites.emplace_back(commonMaterialsBufferWrite);


    std::vector<VkDescriptorImageInfo> imageInfos(s_sceneImageViews.size());
    imageInfos.clear();

    for (size_t i = 0; i < s_sceneImageViews.size(); ++i) {
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageView = s_sceneImageViews[i].Get();
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imageInfos.emplace_back(imageInfo);

        VkWriteDescriptorSet texWrite = {};
        texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        texWrite.dstSet = s_commonDescriptorSet;
        texWrite.dstBinding = COMMON_MTL_TEXTURES_DESCRIPTOR_SLOT;
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
        texWrite.dstSet = s_commonDescriptorSet;
        texWrite.dstBinding = COMMON_MTL_TEXTURES_DESCRIPTOR_SLOT;
        texWrite.dstArrayElement = i;
        texWrite.descriptorCount = 1;
        texWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        texWrite.pImageInfo = &emptyTexInfo;

        descWrites.emplace_back(texWrite);
    }


    VkDescriptorBufferInfo commonInstInfoBufferInfo = {};
    commonInstInfoBufferInfo.buffer = s_commonInstDataBuffer.Get();
    commonInstInfoBufferInfo.offset = 0;
    commonInstInfoBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet commonInstInfoBufferWrite = {};
    commonInstInfoBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    commonInstInfoBufferWrite.dstSet = s_commonDescriptorSet;
    commonInstInfoBufferWrite.dstBinding = COMMON_INST_INFOS_DESCRIPTOR_SLOT;
    commonInstInfoBufferWrite.dstArrayElement = 0;
    commonInstInfoBufferWrite.descriptorCount = 1;
    commonInstInfoBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    commonInstInfoBufferWrite.pBufferInfo = &commonInstInfoBufferInfo;

    descWrites.emplace_back(commonInstInfoBufferWrite);


    VkDescriptorBufferInfo commonVertDataBufferInfo = {};
    commonVertDataBufferInfo.buffer = s_vertexBuffer.Get();
    commonVertDataBufferInfo.offset = 0;
    commonVertDataBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet commonVertDataBufferWrite = {};
    commonVertDataBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    commonVertDataBufferWrite.dstSet = s_commonDescriptorSet;
    commonVertDataBufferWrite.dstBinding = COMMON_VERTEX_DATA_DESCRIPTOR_SLOT;
    commonVertDataBufferWrite.dstArrayElement = 0;
    commonVertDataBufferWrite.descriptorCount = 1;
    commonVertDataBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    commonVertDataBufferWrite.pBufferInfo = &commonVertDataBufferInfo;

    descWrites.emplace_back(commonVertDataBufferWrite);


    VkDescriptorBufferInfo drawIndirectCommandsBufferInfo = {};
    drawIndirectCommandsBufferInfo.buffer = s_drawIndirectCommandsBuffer.Get();
    drawIndirectCommandsBufferInfo.offset = 0;
    drawIndirectCommandsBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet drawIndirectCommandsBufferWrite = {};
    drawIndirectCommandsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawIndirectCommandsBufferWrite.dstSet = s_commonDescriptorSet;
    drawIndirectCommandsBufferWrite.dstBinding = BASE_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT;
    drawIndirectCommandsBufferWrite.dstArrayElement = 0;
    drawIndirectCommandsBufferWrite.descriptorCount = 1;
    drawIndirectCommandsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    drawIndirectCommandsBufferWrite.pBufferInfo = &drawIndirectCommandsBufferInfo;

    descWrites.emplace_back(drawIndirectCommandsBufferWrite);

    VkDescriptorBufferInfo drawIndirectCommandsCountBufferInfo = {};
    drawIndirectCommandsCountBufferInfo.buffer = s_drawIndirectCommandsCountBuffer.Get();
    drawIndirectCommandsCountBufferInfo.offset = 0;
    drawIndirectCommandsCountBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet drawIndirectCommandsCountBufferWrite = {};
    drawIndirectCommandsCountBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawIndirectCommandsCountBufferWrite.dstSet = s_commonDescriptorSet;
    drawIndirectCommandsCountBufferWrite.dstBinding = BASE_INDIRECT_DRAW_CMDS_COUNT_DESCRIPTOR_SLOT;
    drawIndirectCommandsCountBufferWrite.dstArrayElement = 0;
    drawIndirectCommandsCountBufferWrite.descriptorCount = 1;
    drawIndirectCommandsCountBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    drawIndirectCommandsCountBufferWrite.pBufferInfo = &drawIndirectCommandsCountBufferInfo;

    descWrites.emplace_back(drawIndirectCommandsCountBufferWrite);
    
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


static void CmdPipelineBufferBarrier(
    vkn::CmdBuffer& cmdBuffer,
    VkPipelineStageFlags2 srcStageMask, 
    VkPipelineStageFlags2 dstStageMask,
    VkAccessFlags2 srcAccessMask, 
    VkAccessFlags2 dstAccessMask,
    VkBuffer buffer,
    VkDeviceSize offset = 0,
    VkDeviceSize size = VK_WHOLE_SIZE
) {
    VkBufferMemoryBarrier2 bufferBarrier2 = {};
    bufferBarrier2.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferBarrier2.srcStageMask = srcStageMask;
    bufferBarrier2.srcAccessMask = srcAccessMask;
    bufferBarrier2.dstStageMask = dstStageMask;
    bufferBarrier2.dstAccessMask = dstAccessMask;
    bufferBarrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier2.buffer = buffer;
    bufferBarrier2.offset = offset;
    bufferBarrier2.size = size;

    VkDependencyInfo vkDependencyInfo = {};
    vkDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    vkDependencyInfo.bufferMemoryBarrierCount = 1;
    vkDependencyInfo.pBufferMemoryBarriers = &bufferBarrier2;

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


#ifndef USE_FASTGLTF
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


static void LoadSceneMaterials(const gltf::Model& model)
{
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

        vkn::AllocationInfo stagingTexBufAI = {};
        stagingTexBufAI.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        stagingTexBufAI.usage = VMA_MEMORY_USAGE_AUTO;

        vkn::BufferCreateInfo stagingTexBufCI = {};
        stagingTexBufCI.pDevice = &s_vkDevice;
        stagingTexBufCI.size = gltfImage.image.size() * sizeof(gltfImage.image[0]);
        stagingTexBufCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingTexBufCI.pAllocInfo = &stagingTexBufAI;

        vkn::Buffer& stagingTexBuffer = stagingSceneImageBuffers[texIdx];
        stagingTexBuffer.Create(stagingTexBufCI);
        CORE_ASSERT(stagingTexBuffer.IsCreated());

        void* pImageData = stagingTexBuffer.Map(0, VK_WHOLE_SIZE);
        memcpy(pImageData, gltfImage.image.data(), stagingTexBufCI.size);
        stagingTexBuffer.Unmap();

        vkn::AllocationInfo imageAI = {};
        imageAI.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
        imageAI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        vkn::ImageCreateInfo imageCI = {};

        imageCI.pDevice = &s_vkDevice;
        imageCI.type = VK_IMAGE_TYPE_2D;
        imageCI.extent.width = gltfImage.width;
        imageCI.extent.height = gltfImage.height;
        imageCI.extent.depth = 1;
        imageCI.format = gltf::GetImageVkFormat(gltfImage.component, gltfImage.pixel_type, isSRGB);
        imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCI.mipLevels = 1;
        imageCI.arrayLayers = 1;
        imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCI.pAllocInfo = &imageAI;

        vkn::Image& sceneImage = s_sceneImages[texIdx];
        sceneImage.Create(imageCI);
        CORE_ASSERT(sceneImage.IsCreated());
        sceneImage.SetDebugName("COMMON_MTL_TEXTURE_%zu", texIdx);

        vkn::ImageViewCreateInfo viewCI = {};

        viewCI.pOwner = &sceneImage;
        viewCI.type = VK_IMAGE_VIEW_TYPE_2D;
        viewCI.format = imageCI.format;
        viewCI.components.r = VK_COMPONENT_SWIZZLE_R;
        viewCI.components.g = VK_COMPONENT_SWIZZLE_G;
        viewCI.components.b = VK_COMPONENT_SWIZZLE_B;
        viewCI.components.a = VK_COMPONENT_SWIZZLE_A;
        viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCI.subresourceRange.baseMipLevel = 0;
        viewCI.subresourceRange.baseArrayLayer = 0;
        viewCI.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        viewCI.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        vkn::ImageView& view = s_sceneImageViews[texIdx];
        view.Create(viewCI);
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

    vkn::AllocationInfo commonMtlBuffAI = {};
    commonMtlBuffAI.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    commonMtlBuffAI.usage = VMA_MEMORY_USAGE_AUTO;

    vkn::BufferCreateInfo commonMtlBuffCI = {};
    commonMtlBuffCI.pDevice = &s_vkDevice;
    commonMtlBuffCI.size = s_sceneMaterials.size() * sizeof(COMMON_MATERIAL);
    commonMtlBuffCI.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    commonMtlBuffCI.pAllocInfo = &commonMtlBuffAI;

    s_commonMaterialsBuffer.Create(commonMtlBuffCI);
    CORE_ASSERT(s_commonMaterialsBuffer.IsCreated());
    s_commonMaterialsBuffer.SetDebugName("COMMON_MATERIALS");

    void* pCommonMaterialsData = s_commonMaterialsBuffer.Map(0, VK_WHOLE_SIZE);
    memcpy(pCommonMaterialsData, s_sceneMaterials.data(), s_sceneMaterials.size() * sizeof(COMMON_MATERIAL));
    s_commonMaterialsBuffer.Unmap();

    vkn::AllocationInfo defTexAI = {};
    defTexAI.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    defTexAI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::ImageCreateInfo defTexCI = {};

    defTexCI.pDevice = &s_vkDevice;
    defTexCI.type = VK_IMAGE_TYPE_2D;
    defTexCI.extent.width = 1;
    defTexCI.extent.height = 1;
    defTexCI.extent.depth = 1;
    defTexCI.format = VK_FORMAT_R8_UNORM;
    defTexCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    defTexCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    defTexCI.mipLevels = 1;
    defTexCI.arrayLayers = 1;
    defTexCI.samples = VK_SAMPLE_COUNT_1_BIT;
    defTexCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    defTexCI.pAllocInfo = &defTexAI;

    s_sceneDefaultImage.Create(defTexCI);
    CORE_ASSERT(s_sceneDefaultImage.IsCreated());
    s_sceneDefaultImage.SetDebugName("DEFAULT_TEX");

    vkn::ImageViewCreateInfo defTexViewCI = {};

    defTexViewCI.pOwner = &s_sceneDefaultImage;
    defTexViewCI.type = VK_IMAGE_VIEW_TYPE_2D;
    defTexViewCI.format = defTexCI.format;
    defTexViewCI.components.r = VK_COMPONENT_SWIZZLE_R;
    defTexViewCI.components.g = VK_COMPONENT_SWIZZLE_G;
    defTexViewCI.components.b = VK_COMPONENT_SWIZZLE_B;
    defTexViewCI.components.a = VK_COMPONENT_SWIZZLE_A;
    defTexViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    defTexViewCI.subresourceRange.baseMipLevel = 0;
    defTexViewCI.subresourceRange.baseArrayLayer = 0;
    defTexViewCI.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    defTexViewCI.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    s_sceneDefaultImageView.Create(defTexViewCI);
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
            texRegion.imageExtent = s_sceneImages[i].GetSize();

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

    CORE_LOG_INFO("Materials loading finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void LoadSceneMeshInfos(const gltf::Model& model)
{
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

    std::vector<VertexPacked> cpuVertBuffer;
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

    std::vector<IndexType> cpuIndexBuffer;
    cpuIndexBuffer.reserve(indexCount);

    s_sceneMeshInfos.reserve(model.meshes.size());
    s_sceneMeshInfos.clear();

    auto GetAttribPtr = [](const gltf::Model& model, const gltf::Primitive& primitive, const char* pAttribName, size_t& count, size_t& stride) -> const uint8_t*
    {
        CORE_ASSERT(pAttribName != nullptr);
        CORE_ASSERT(primitive.attributes.contains(pAttribName));
        
        const uint32_t accessorIndex = primitive.attributes.at(pAttribName);
        const gltf::Accessor& accessor = model.accessors[accessorIndex];
        
        const auto& bufferView = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[bufferView.buffer];

        count = accessor.count;
        stride = accessor.ByteStride(bufferView);

        return buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
    };

    for (const gltf::Mesh& m : model.meshes) {
        for (const gltf::Primitive& primitive : m.primitives) {
            COMMON_MESH_INFO mesh = {};

            mesh.FIRST_VERTEX = cpuVertBuffer.size();
            mesh.FIRST_INDEX = cpuIndexBuffer.size();

            mesh.BOUNDS_MIN_LCS = glm::vec3(FLT_MAX);
            mesh.BOUNDS_MAX_LCS = glm::vec3(-FLT_MAX);

            const IndexType primitiveStartIndex = cpuVertBuffer.size();

            size_t positionsCount = 0; size_t posStride = 0;
            const uint8_t* pPositionData = GetAttribPtr(model, primitive, "POSITION", positionsCount, posStride);
            
            size_t normalsCount = 0; size_t normalStride = 0;
            const uint8_t* pNormalData = GetAttribPtr(model, primitive, "NORMAL", normalsCount, normalStride);
            
            size_t texcoordsCount = 0; size_t texcoordStride = 0;
            const uint8_t* pTexcoordData = GetAttribPtr(model, primitive, "TEXCOORD_0", texcoordsCount, texcoordStride);

            mesh.VERTEX_COUNT += positionsCount;

            for (size_t i = 0; i < positionsCount; ++i) {
                const float* pPosition = reinterpret_cast<const float*>(pPositionData + i * posStride);
                const float* pNormal = reinterpret_cast<const float*>(pNormalData + i * normalStride);
                const float* pTexcoord = reinterpret_cast<const float*>(pTexcoordData + i * texcoordStride);
                
                const glm::vec3 position(pPosition[0], pPosition[1], pPosition[2]);
                const glm::vec3 normal(pNormal[0], pNormal[1], pNormal[2]);
                const glm::vec2 texcoord(pTexcoord[0], pTexcoord[1]);

                mesh.BOUNDS_MIN_LCS = glm::min(mesh.BOUNDS_MIN_LCS, position);
                mesh.BOUNDS_MAX_LCS = glm::max(mesh.BOUNDS_MAX_LCS, position);

                VertexPacked vertex = {};

                vertex.posXY = glm::packHalf2x16(glm::vec2(position.x, position.y));
                vertex.posZnormX = glm::packHalf2x16(glm::vec2(position.z, normal.x));
                vertex.normYZ = glm::packHalf2x16(glm::vec2(normal.y, normal.z));
                vertex.texcoord = glm::packHalf2x16(texcoord);

                cpuVertBuffer.emplace_back(vertex);
            }

            CORE_ASSERT_MSG(primitive.indices >= 0, "GLTF primitive must have index accessor");

            const gltf::Accessor& indexAccessor = model.accessors[primitive.indices];
            const gltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
            const gltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

            const uint8_t* pIndexData = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;

            mesh.INDEX_COUNT += indexAccessor.count;

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

                CORE_ASSERT_MSG(index < std::numeric_limits<IndexType>::max(), "Vertex index is greater than %zu", std::numeric_limits<IndexType>::max());
                cpuIndexBuffer.push_back(static_cast<IndexType>(index));
            }

            s_sceneMeshInfos.emplace_back(mesh);
        }
    }

    vkn::AllocationInfo stagingBufAI = {};
    stagingBufAI.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    stagingBufAI.usage = VMA_MEMORY_USAGE_AUTO;

    vkn::BufferCreateInfo stagingBufCI = {};
    stagingBufCI.pDevice = &s_vkDevice;
    stagingBufCI.size = cpuVertBuffer.size() * sizeof(VertexPacked);
    stagingBufCI.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufCI.pAllocInfo = &stagingBufAI;

    vkn::Buffer stagingVertBuffer(stagingBufCI);
    CORE_ASSERT(stagingVertBuffer.IsCreated());
    stagingVertBuffer.SetDebugName("STAGING_VERT_BUFFER");

    {
        void* pVertexBufferData = stagingVertBuffer.Map(0, VK_WHOLE_SIZE);
        memcpy(pVertexBufferData, cpuVertBuffer.data(), cpuVertBuffer.size() * sizeof(VertexPacked));
        stagingVertBuffer.Unmap();
    }

    stagingBufCI.size = cpuIndexBuffer.size() * sizeof(IndexType);

    vkn::Buffer stagingIndexBuffer(stagingBufCI);
    CORE_ASSERT(stagingIndexBuffer.IsCreated());
    stagingIndexBuffer.SetDebugName("STAGING_IDX_BUFFER");

    {
        void* pIndexBufferData = stagingIndexBuffer.Map(0, VK_WHOLE_SIZE);
        memcpy(pIndexBufferData, cpuIndexBuffer.data(), cpuIndexBuffer.size() * sizeof(IndexType));
        stagingIndexBuffer.Unmap();
    }

    stagingBufCI.size = s_sceneMeshInfos.size() * sizeof(COMMON_MESH_INFO);

    vkn::Buffer stagingMeshInfosBuffer(stagingBufCI);
    CORE_ASSERT(stagingMeshInfosBuffer.IsCreated());
    stagingMeshInfosBuffer.SetDebugName("STAGING_MESH_INFOS_BUFFER");

    {
        void* pMeshBufferData = stagingMeshInfosBuffer.Map(0, VK_WHOLE_SIZE);
        memcpy(pMeshBufferData, s_sceneMeshInfos.data(), s_sceneMeshInfos.size() * sizeof(COMMON_MESH_INFO));
        stagingMeshInfosBuffer.Unmap();
    }

    vkn::AllocationInfo vertBufAI = {};
    vertBufAI.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    vertBufAI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo vertBufCI = {};
    vertBufCI.pDevice = &s_vkDevice;
    vertBufCI.size = cpuVertBuffer.size() * sizeof(VertexPacked);
    vertBufCI.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertBufCI.pAllocInfo = &vertBufAI;

    s_vertexBuffer.Create(vertBufCI);
    CORE_ASSERT(s_vertexBuffer.IsCreated());
    s_vertexBuffer.SetDebugName("COMMON_VB");

    vkn::AllocationInfo idxBufAI = {};
    idxBufAI.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    idxBufAI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo idxBufCI = {};
    idxBufCI.pDevice = &s_vkDevice;
    idxBufCI.size = cpuIndexBuffer.size() * sizeof(IndexType);
    idxBufCI.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    idxBufCI.pAllocInfo = &idxBufAI;

    s_indexBuffer.Create(idxBufCI);
    CORE_ASSERT(s_indexBuffer.IsCreated());
    s_indexBuffer.SetDebugName("COMMON_IB");

    vkn::AllocationInfo meshInfosBufAI = {};
    meshInfosBufAI.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    meshInfosBufAI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo meshInfosBufCI = {};
    meshInfosBufCI.pDevice = &s_vkDevice;
    meshInfosBufCI.size = s_sceneMeshInfos.size() * sizeof(COMMON_MESH_INFO);
    meshInfosBufCI.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    meshInfosBufCI.pAllocInfo = &meshInfosBufAI;
    
    s_commonMeshDataBuffer.Create(meshInfosBufCI);
    CORE_ASSERT(s_commonMeshDataBuffer.IsCreated());
    s_commonMeshDataBuffer.SetDebugName("COMMON_MESH_INFOS");


    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        VkBufferCopy bufferRegion = {};
        
        bufferRegion.size = cpuVertBuffer.size() * sizeof(VertexPacked);
        vkCmdCopyBuffer(cmdBuffer.Get(), stagingVertBuffer.Get(), s_vertexBuffer.Get(), 1, &bufferRegion);

        bufferRegion.size = cpuIndexBuffer.size() * sizeof(IndexType);
        vkCmdCopyBuffer(cmdBuffer.Get(), stagingIndexBuffer.Get(), s_indexBuffer.Get(), 1, &bufferRegion);
        
        bufferRegion.size = s_sceneMeshInfos.size() * sizeof(COMMON_MESH_INFO);
        vkCmdCopyBuffer(cmdBuffer.Get(), stagingMeshInfosBuffer.Get(), s_commonMeshDataBuffer.Get(), 1, &bufferRegion);
    });

    CORE_LOG_INFO("Mesh loading finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void LoadSceneTransforms(const gltf::Model& model)
{
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

    vkn::AllocationInfo stagingBufAI = {};
    stagingBufAI.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    stagingBufAI.usage = VMA_MEMORY_USAGE_AUTO;

    vkn::BufferCreateInfo stagingBufCI = {};
    stagingBufCI.pDevice = &s_vkDevice;
    stagingBufCI.size = s_sceneTransforms.size() * sizeof(COMMON_TRANSFORM);
    stagingBufCI.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufCI.pAllocInfo = &stagingBufAI;

    vkn::Buffer stagingBuffer(stagingBufCI);
    CORE_ASSERT(stagingBuffer.IsCreated());
    stagingBuffer.SetDebugName("STAGING_TRANSFORM_BUFFER");

    {
        void* pData = stagingBuffer.Map(0, VK_WHOLE_SIZE);
        memcpy(pData, s_sceneTransforms.data(), s_sceneTransforms.size() * sizeof(COMMON_TRANSFORM));
        stagingBuffer.Unmap();
    }

    vkn::AllocationInfo commonTrsBufAI = {};
    commonTrsBufAI.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    commonTrsBufAI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo commonTrsCI = {};
    commonTrsCI.pDevice = &s_vkDevice;
    commonTrsCI.size = s_sceneTransforms.size() * sizeof(COMMON_TRANSFORM);
    commonTrsCI.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    commonTrsCI.pAllocInfo = &commonTrsBufAI;

    s_commonTransformsBuffer.Create(commonTrsCI);
    CORE_ASSERT(s_commonTransformsBuffer.IsCreated());
    s_commonTransformsBuffer.SetDebugName("COMMON_TRANSFORMS");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        VkBufferCopy bufferRegion = {};
        bufferRegion.size = s_sceneTransforms.size() * sizeof(COMMON_TRANSFORM);
        
        vkCmdCopyBuffer(cmdBuffer.Get(), stagingBuffer.Get(), s_commonTransformsBuffer.Get(), 1, &bufferRegion);
    });

    CORE_LOG_INFO("Transforms loading finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void LoadSceneInstInfos(const gltf::Model& model)
{
    Timer timer;

    s_sceneInstInfos.reserve(model.meshes.size());
    s_sceneInstInfos.clear();

    uint32_t meshIdx = 0;

    for (const gltf::Mesh& m : model.meshes) {
        for (const gltf::Primitive& primitive : m.primitives) {
            COMMON_INST_INFO instInfo = {};
            
            instInfo.MESH_IDX = meshIdx;
            instInfo.MATERIAL_IDX = primitive.material;

            s_sceneInstInfos.emplace_back(instInfo);
            ++meshIdx;
        }
    }

    size_t instInfoIdx = 0;

    for (size_t meshGroupIndex = 0; meshGroupIndex < model.meshes.size(); ++meshGroupIndex) {
        const gltf::Mesh& mesh = model.meshes[meshGroupIndex];

        for (size_t meshIdx = 0; meshIdx < mesh.primitives.size(); ++meshIdx) {
            s_sceneInstInfos[instInfoIdx].TRANSFORM_IDX = meshGroupIndex;
            ++instInfoIdx;
        }
    }

    vkn::AllocationInfo stagingBufAI = {};
    stagingBufAI.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    stagingBufAI.usage = VMA_MEMORY_USAGE_AUTO;

    vkn::BufferCreateInfo stagingBufCI = {};
    stagingBufCI.pDevice = &s_vkDevice;
    stagingBufCI.size = s_sceneInstInfos.size() * sizeof(COMMON_INST_INFO);
    stagingBufCI.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufCI.pAllocInfo = &stagingBufAI;

    vkn::Buffer stagingBuffer(stagingBufCI);
    CORE_ASSERT(stagingBuffer.IsCreated());
    stagingBuffer.SetDebugName("STAGING_INST_INFOS_BUFFER");

    {
        void* pData = stagingBuffer.Map(0, VK_WHOLE_SIZE);
        memcpy(pData, s_sceneInstInfos.data(), s_sceneInstInfos.size() * sizeof(COMMON_INST_INFO));
        stagingBuffer.Unmap();
    }

    vkn::AllocationInfo instInfosBufAI = {};
    instInfosBufAI.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    instInfosBufAI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo instInfosBufCI = {};
    instInfosBufCI.pDevice = &s_vkDevice;
    instInfosBufCI.size = s_sceneInstInfos.size() * sizeof(COMMON_INST_INFO);
    instInfosBufCI.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    instInfosBufCI.pAllocInfo = &instInfosBufAI;

    s_commonInstDataBuffer.Create(instInfosBufCI);
    CORE_ASSERT(s_commonInstDataBuffer.IsCreated());
    s_commonInstDataBuffer.SetDebugName("COMMON_INST_INFOS");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        VkBufferCopy bufferRegion = {};
        bufferRegion.size = s_sceneInstInfos.size() * sizeof(COMMON_INST_INFO);
        
        vkCmdCopyBuffer(cmdBuffer.Get(), stagingBuffer.Get(), s_commonInstDataBuffer.Get(), 1, &bufferRegion);
    });

    CORE_LOG_INFO("Instance infos loading finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void LoadScene(const fs::path& filepath)
{
    const std::string strPath = filepath.string();

    if (!fs::exists(filepath)) {
		CORE_ASSERT_FAIL("Unknown scene path: %s", strPath.c_str());
		return;
	}

    ENG_PROFILE_SCOPED_MARKER_C("Load_Scene", 255, 50, 255, 255);
    
    CORE_LOG_TRACE("Loading \"%s\"...", strPath.c_str());

    Timer timer;

    gltf::TinyGLTF modelLoader;
    gltf::Model model;
    std::string error, warning;

    const bool isModelLoaded = filepath.extension() == ".gltf" ? 
        modelLoader.LoadASCIIFromFile(&model, &error, &warning, strPath) :
        modelLoader.LoadBinaryFromFile(&model, &error, &warning, strPath);

    if (!warning.empty()) {
        CORE_LOG_WARN("Warning during %s model loading: %s", strPath.c_str(), warning.c_str());
    }
    CORE_ASSERT_MSG(isModelLoaded && error.empty(), "Failed to load %s model: %s", strPath.c_str(), error.c_str());

    LoadSceneTransforms(model);
    LoadSceneMaterials(model);
    LoadSceneMeshInfos(model);
    LoadSceneInstInfos(model);

    CORE_LOG_INFO("\"%s\" loading finished: %f ms", strPath.c_str(), timer.End().GetDuration<float, std::milli>());
}
#else
static void LoadSceneTexturesData(const gltf::Asset& asset, const fs::path& fileDir)
{
    Timer timer;

    s_cpuTexturesData.reserve(asset.images.size());
    s_cpuTexturesData.clear();

    struct FreeImageLoadInfo
    {
        FreeImageLoadInfo(const fs::path& fileDir, const gltf::sources::URI& filePath)
        {
            const fs::path path = filePath.uri.isLocalPath() ? fs::absolute(fileDir / filePath.uri.fspath()) : filePath.uri.fspath();
            const fs::path extension = path.extension();
            
            filepath = path.string();

            if (extension == ".png") {
                fmt = FIF_PNG;
                flags = PNG_DEFAULT;
            } else if (extension == ".jpeg" || extension == ".jpg") {
                fmt = FIF_JPEG;
                flags = JPEG_DEFAULT;
            } else if (extension == ".hdr") {
                fmt = FIF_HDR;
                flags = HDR_DEFAULT;
            } else {
                CORE_ASSERT_FAIL("Unsupported texture extension: %s", extension.string().c_str());
            }
        }

        std::string filepath;
        FREE_IMAGE_FORMAT fmt = FIF_UNKNOWN;
        int32_t flags = 0;
    };

    auto LoadImageFromMemory = [](TextureLoadData& outTexData, const void* pData, size_t dataSize)
    {
        FIMEMORY data = { (void*)(pData) };

        FREE_IMAGE_FORMAT fmt = FreeImage_GetFileTypeFromMemory(&data, static_cast<int>(dataSize));
        CORE_ASSERT_MSG(fmt != FIF_UNKNOWN, "Failed to detect image format");

        outTexData.pBitmap = FreeImage_LoadFromMemory(fmt, &data, static_cast<int>(dataSize));
        CORE_ASSERT_MSG(outTexData.pBitmap, "Failed to load image from memory");
    };

    for (const gltf::Image& image : asset.images) {
        TextureLoadData texData = {};
        
    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        memcpy_s(texData.name.data(), texData.name.size() - 1, image.name.c_str(), image.name.size());
    #endif
        
        std::visit(
            gltf::visitor {
                [](const auto& arg){},
                [&](const gltf::sources::URI& filePath) {    
                    FreeImageLoadInfo loadInfo(fileDir, filePath);

                    texData.pBitmap = FreeImage_Load(loadInfo.fmt, loadInfo.filepath.c_str(), loadInfo.flags);
                    CORE_ASSERT_MSG(texData.pBitmap, "Failed to load %s texture data", loadInfo.filepath.c_str());

                    s_cpuTexturesData.emplace_back(std::move(texData));
                },
                [&](const gltf::sources::Vector& vector) {
                    LoadImageFromMemory(texData, vector.bytes.data(), vector.bytes.size());
                    s_cpuTexturesData.emplace_back(std::move(texData));
                },
                [&](const gltf::sources::BufferView& view) {
                    const gltf::BufferView& bufferView = asset.bufferViews[view.bufferViewIndex];
                    const gltf::Buffer& buffer = asset.buffers[bufferView.bufferIndex];
    
                    std::visit(gltf::visitor {
                        [](const auto& arg){},
                        [&](const gltf::sources::Vector& vector) {
                            LoadImageFromMemory(texData, vector.bytes.data(), vector.bytes.size());
                            s_cpuTexturesData.emplace_back(std::move(texData));
                        }
                    },
                    buffer.data);
                },
            },
        image.data);
    }

    CORE_LOG_INFO("FastGLTF: Textures data loading finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void LoadSceneMaterials(const gltf::Asset& asset)
{
    Timer timer;

    s_cpuMaterials.reserve(asset.materials.size());
    s_cpuMaterials.clear();

    for (const gltf::Material& material : asset.materials) {
        Material mtl = {};

        if (material.pbrData.baseColorTexture.has_value()) {
            mtl.albedoTexIdx = material.pbrData.baseColorTexture.value().textureIndex;
        }

        if (material.normalTexture.has_value()) {
            mtl.normalTexIdx = material.normalTexture.value().textureIndex;
        }
        
        if (material.pbrData.metallicRoughnessTexture.has_value()) {
            mtl.metalRoughTexIdx = material.pbrData.metallicRoughnessTexture.value().textureIndex;
        }
        
        if (material.occlusionTexture.has_value()) {
            mtl.aoTexIdx = material.occlusionTexture.value().textureIndex;
        }

        if (material.emissiveTexture.has_value()) {
            mtl.emissiveTexIdx = material.emissiveTexture.value().textureIndex;
        }

        s_cpuMaterials.emplace_back(mtl);
    }

    CORE_LOG_INFO("FastGLTF: Materials data loading finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void LoadSceneMeshData(const gltf::Asset& asset)
{
    Timer timer;

    size_t vertexCount = 0;
    size_t indexCount = 0;
    size_t meshesCount = 0;
    size_t dipCount = 0;

    const size_t dipGroupCount = asset.meshes.size();

    auto GetVertexAttribAccessor = [](const gltf::Asset& asset, const gltf::Primitive& primitive, std::string_view name) -> const gltf::Accessor*
    {
        const fastgltf::Attribute* pAttrib = primitive.findAttribute(name); 
        return pAttrib != primitive.attributes.cend() ? &asset.accessors[pAttrib->accessorIndex] : nullptr;
    };

    for (const gltf::Mesh& mesh : asset.meshes) {
        const size_t submeshCount = mesh.primitives.size();

        meshesCount += submeshCount;
        dipCount += submeshCount;

        for (size_t primIdx = 0; primIdx < submeshCount; ++primIdx) {
            const gltf::Primitive& primitive = mesh.primitives[primIdx];

            const gltf::Accessor* pPosAccessor = GetVertexAttribAccessor(asset, primitive, "POSITION");
            CORE_ASSERT_MSG(pPosAccessor != nullptr, "Failed to find POSITION vertex attribute accessor for %zu primitive of %s mesh", primIdx, mesh.name.c_str());
            
            vertexCount += pPosAccessor->count;

            CORE_ASSERT_MSG(primitive.indicesAccessor.has_value(), "%zu primitive of %s mesh doesn't contation index accessor", primIdx, mesh.name.c_str());
            
            const gltf::Accessor& indexAccessor = asset.accessors[primitive.indicesAccessor.value()];
            indexCount += indexAccessor.count;
        }
    }

    s_cpuVertexBuffer.reserve(vertexCount);
    s_cpuVertexBuffer.clear();

    s_cpuIndexBuffer.reserve(indexCount);
    s_cpuIndexBuffer.clear();

    s_cpuMeshes.reserve(meshesCount);
    s_cpuMeshes.clear();

    s_cpuDIPGroups.reserve(dipGroupCount);
    s_cpuDIPGroups.clear();

    s_cpuDIPs.reserve(meshesCount);
    s_cpuDIPs.clear();

    size_t sceneIdx = 0;

    gltf::iterateSceneNodes(asset, sceneIdx, gltf::math::fmat4x4(), 
        [&](auto&& node, auto&& nodeTrs) {            
            if (!node.meshIndex.has_value()) {
                return;
            }

            const gltf::Mesh& mesh = asset.meshes[node.meshIndex.value()];

            DIPGroup dipGroup = {};
            dipGroup.firstDIP = s_cpuDIPs.size();

            for (size_t primIdx = 0; primIdx < mesh.primitives.size(); ++primIdx) {
                const gltf::Primitive& primitive = mesh.primitives[primIdx];
                
                const gltf::Accessor* pPosAccessor = GetVertexAttribAccessor(asset, primitive, "POSITION");
                CORE_ASSERT_MSG(pPosAccessor != nullptr, "Failed to find POSITION vertex attribute accessor for %zu primitive of %s mesh", primIdx, mesh.name.c_str());

                const gltf::Accessor* pNormAccessor = GetVertexAttribAccessor(asset, primitive, "NORMAL");
                CORE_ASSERT_MSG(pNormAccessor != nullptr, "Failed to find NORMAL vertex attribute accessor for %zu primitive of %s mesh", primIdx, mesh.name.c_str());
                
                const gltf::Accessor* pUvAccessor = GetVertexAttribAccessor(asset, primitive, "TEXCOORD_0");
                CORE_ASSERT_MSG(pUvAccessor != nullptr, "Failed to find TEXCOORD_0 vertex attribute accessor for %zu primitive of %s mesh", primIdx, mesh.name.c_str());

                CORE_ASSERT(pPosAccessor->count == pNormAccessor->count && pPosAccessor->count == pUvAccessor->count);

                Mesh cpuMesh = {};

                cpuMesh.firstVertex = s_cpuVertexBuffer.size();
                cpuMesh.vertexCount = pPosAccessor->count;

                for (size_t vertIdx = 0; vertIdx < pPosAccessor->count; ++vertIdx) {
                    const glm::vec3 lpos = gltf::getAccessorElement<glm::vec3>(asset, *pPosAccessor, vertIdx);
                    const glm::vec3 lnorm = gltf::getAccessorElement<glm::vec3>(asset, *pNormAccessor, vertIdx);
                    const glm::vec2 uv = gltf::getAccessorElement<glm::vec2>(asset, *pUvAccessor, vertIdx);
                    
                    Vertex vertex = {};
                    vertex.Pack(lpos, lnorm, uv);

                    s_cpuVertexBuffer.emplace_back(vertex);
                }

                CORE_ASSERT(pPosAccessor->min.has_value());
                const auto& aabbLCSMin = pPosAccessor->min.value();
                CORE_ASSERT(aabbLCSMin.size() == 3);

                CORE_ASSERT(pPosAccessor->max.has_value());
                const auto& aabbLCSMax = pPosAccessor->max.value();
                CORE_ASSERT(aabbLCSMax.size() == 3);

                cpuMesh.bounds = {};
                if (aabbLCSMin.isType<std::int64_t>()) {
                    cpuMesh.bounds.min = glm::vec3(aabbLCSMin.get<std::int64_t>(0), aabbLCSMin.get<std::int64_t>(1), aabbLCSMin.get<std::int64_t>(2));
                } else {
                    cpuMesh.bounds.min = glm::vec3(aabbLCSMin.get<double>(0), aabbLCSMin.get<double>(1), aabbLCSMin.get<double>(2));
                }
                
                if (aabbLCSMax.isType<std::int64_t>()) {
                    cpuMesh.bounds.max = glm::vec3(aabbLCSMax.get<std::int64_t>(0), aabbLCSMax.get<std::int64_t>(1), aabbLCSMax.get<std::int64_t>(2));
                } else {
                    cpuMesh.bounds.max = glm::vec3(aabbLCSMax.get<double>(0), aabbLCSMax.get<double>(1), aabbLCSMax.get<double>(2));
                }

                CORE_ASSERT_MSG(primitive.indicesAccessor.has_value(), "%zu primitive of %s mesh doesn't contation index accessor", primIdx, mesh.name.c_str());
                const gltf::Accessor& indexAccessor = asset.accessors[primitive.indicesAccessor.value()];

                CORE_ASSERT_MSG(indexAccessor.type == fastgltf::AccessorType::Scalar, "%zu primitive of %s mesh has invalid index accessor type", primIdx, mesh.name.c_str());
                
                cpuMesh.firstIndex = s_cpuIndexBuffer.size();
                cpuMesh.indexCount = indexAccessor.count;
                
                if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedShort) {
                    gltf::iterateAccessor<uint16_t>(asset, indexAccessor, 
                        [&](uint16_t index) {
                            s_cpuIndexBuffer.emplace_back(index);
                        }
                    );
                } else if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedInt) {
                    gltf::iterateAccessor<uint32_t>(asset, indexAccessor, 
                        [&](uint32_t index) {
                            s_cpuIndexBuffer.emplace_back(index);
                        }
                    );
                }

                s_cpuMeshes.emplace_back(cpuMesh);

                DIP dip = {};
                dip.meshIdx = s_cpuMeshes.size() - 1;
                dip.mtlIdx = primitive.materialIndex.has_value() ? primitive.materialIndex.value() : -1;

                s_cpuDIPs.emplace_back(dip);

                ++dipGroup.dipCount;
            }

            s_cpuDIPGroups.emplace_back(dipGroup);
        }
    );

    CORE_LOG_INFO("FastGLTF: Mesh loading finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void LoadSceneNodes(const gltf::Asset& asset)
{
    Timer timer;

    s_cpuSceneNodes.reserve(asset.nodes.size());
    s_cpuSceneNodes.clear();

    s_cpuTransforms.reserve(asset.nodes.size());
    s_cpuTransforms.clear();

    size_t sceneIdx = 0;
    CORE_ASSERT_MSG(asset.scenes.size() == 1, "Only one scene per glTF file is allowed for now");

    gltf::iterateSceneNodes(asset, sceneIdx, fastgltf::math::fmat4x4(), 
        [&](auto&& node, auto&& nodeTrs) {
            glm::mat4x4 transform(
                nodeTrs[0][0], nodeTrs[0][1], nodeTrs[0][2], nodeTrs[0][3],
                nodeTrs[1][0], nodeTrs[1][1], nodeTrs[1][2], nodeTrs[1][3],
                nodeTrs[2][0], nodeTrs[2][1], nodeTrs[2][2], nodeTrs[2][3],
                nodeTrs[3][0], nodeTrs[3][1], nodeTrs[3][2], nodeTrs[3][3]
            );
            transform = glm::transpose(transform);

            s_cpuTransforms.emplace_back(glm::mat3x4(transform[0], transform[1], transform[2]));

            SceneNode scnNode = {};

            scnNode.childrenNodes.reserve(node.children.size());
            std::for_each(node.children.cbegin(), node.children.cend(), [&](size_t index) {
                scnNode.childrenNodes.emplace_back(index);
            });
            
            scnNode.name = node.name;
            scnNode.trsIdx = s_cpuTransforms.size() - 1;

            if (node.meshIndex.has_value()) {
                scnNode.dipGroupIdx = node.meshIndex.value();
            }

            s_cpuSceneNodes.emplace_back(std::move(scnNode));
        }
    );

    CORE_LOG_INFO("FastGLTF: Nodes data loading finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void UploadGPUMeshData()
{
    vkn::Buffer& vertStagingBuf = s_commonStagingBuffers[0];
    vkn::Buffer& idxStagingBuf = s_commonStagingBuffers[1];

    void* pVertData = vertStagingBuf.Map(0, VK_WHOLE_SIZE);

    const size_t vertBufSize = s_cpuVertexBuffer.size() * sizeof(Vertex);
    CORE_ASSERT(vertBufSize <= vertStagingBuf.GetMemorySize());
    memcpy(pVertData, s_cpuVertexBuffer.data(), vertBufSize);

    vertStagingBuf.Unmap();

    void* pIndexData = idxStagingBuf.Map(0, VK_WHOLE_SIZE);

    const size_t idxBufSize = s_cpuIndexBuffer.size() * sizeof(IndexType);
    CORE_ASSERT(idxBufSize <= idxStagingBuf.GetMemorySize());
    memcpy(pIndexData, s_cpuIndexBuffer.data(), idxBufSize);

    idxStagingBuf.Unmap();

    vkn::AllocationInfo bufAllocInfo = {};
    bufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    bufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo bufCreateInfo = {};
    bufCreateInfo.pDevice = &s_vkDevice;
    bufCreateInfo.size = vertBufSize;
    bufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufCreateInfo.pAllocInfo = &bufAllocInfo;

    s_vertexBuffer.Create(bufCreateInfo);
    CORE_ASSERT(s_vertexBuffer.IsCreated());
    s_vertexBuffer.SetDebugName("COMMON_VB");

    bufCreateInfo.size = idxBufSize;

    s_indexBuffer.Create(bufCreateInfo);
    CORE_ASSERT(s_indexBuffer.IsCreated());
    s_indexBuffer.SetDebugName("COMMON_IB");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        VkBufferCopy copyRegion = {};
        
        copyRegion.size = vertBufSize;
        vkCmdCopyBuffer(cmdBuffer.Get(), vertStagingBuf.Get(), s_vertexBuffer.Get(), 1, &copyRegion);

        copyRegion.size = idxBufSize;
        vkCmdCopyBuffer(cmdBuffer.Get(), idxStagingBuf.Get(), s_indexBuffer.Get(), 1, &copyRegion);
    });

    vkn::Buffer& meshDataStagingBuf = s_commonStagingBuffers[0];

    COMMON_MESH_INFO* pMeshData = meshDataStagingBuf.Map<COMMON_MESH_INFO>();

    const size_t meshDataBufSize = s_cpuMeshes.size() * sizeof(COMMON_MESH_INFO);
    CORE_ASSERT(meshDataBufSize <= meshDataStagingBuf.GetMemorySize());
    
    for (size_t i = 0; i < s_cpuMeshes.size(); ++i) {
        const Mesh& cpuMesh = s_cpuMeshes[i];

        pMeshData[i].FIRST_VERTEX = cpuMesh.firstVertex;
        pMeshData[i].VERTEX_COUNT = cpuMesh.vertexCount;
        pMeshData[i].FIRST_INDEX  = cpuMesh.firstIndex;
        pMeshData[i].INDEX_COUNT  = cpuMesh.indexCount;
        pMeshData[i].BOUNDS_MIN_LCS  = cpuMesh.bounds.min;
        pMeshData[i].BOUNDS_MAX_LCS  = cpuMesh.bounds.max;
    }

    meshDataStagingBuf.Unmap();

    bufCreateInfo.size = meshDataBufSize;

    s_commonMeshDataBuffer.Create(bufCreateInfo);
    CORE_ASSERT(s_commonMeshDataBuffer.IsCreated());
    s_commonMeshDataBuffer.SetDebugName("COMMON_MESH_DATA");
    
    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        VkBufferCopy copyRegion = {};
        
        copyRegion.size = meshDataBufSize;
        vkCmdCopyBuffer(cmdBuffer.Get(), meshDataStagingBuf.Get(), s_commonMeshDataBuffer.Get(), 1, &copyRegion);
    });
}


static void UploadGPUMaterialData()
{
    vkn::Buffer& mtlDataStagingBuf = s_commonStagingBuffers[0];

    COMMON_MATERIAL* pMtlData = mtlDataStagingBuf.Map<COMMON_MATERIAL>();

    const size_t mtlDataBufSize = s_cpuMaterials.size() * sizeof(COMMON_MATERIAL);
    CORE_ASSERT(mtlDataBufSize <= mtlDataStagingBuf.GetMemorySize());

    for (size_t i = 0; i < s_cpuMaterials.size(); ++i) {
        pMtlData[i].ALBEDO_TEX_IDX = s_cpuMaterials[i].albedoTexIdx;
        pMtlData[i].NORMAL_TEX_IDX = s_cpuMaterials[i].normalTexIdx;
        pMtlData[i].MR_TEX_IDX = s_cpuMaterials[i].metalRoughTexIdx;
        pMtlData[i].AO_TEX_IDX = s_cpuMaterials[i].aoTexIdx;
        pMtlData[i].EMISSIVE_TEX_IDX = s_cpuMaterials[i].emissiveTexIdx;
    }

    mtlDataStagingBuf.Unmap();

    vkn::AllocationInfo bufAllocInfo = {};
    bufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    bufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo bufCreateInfo = {};
    bufCreateInfo.pDevice = &s_vkDevice;
    bufCreateInfo.size = mtlDataBufSize;
    bufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufCreateInfo.pAllocInfo = &bufAllocInfo;

    s_commonMaterialsBuffer.Create(bufCreateInfo);
    CORE_ASSERT(s_commonMaterialsBuffer.IsCreated());
    s_commonMaterialsBuffer.SetDebugName("COMMON_MTL_DATA");
    
    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        VkBufferCopy copyRegion = {};

        copyRegion.size = mtlDataBufSize;
        vkCmdCopyBuffer(cmdBuffer.Get(), mtlDataStagingBuf.Get(), s_commonMaterialsBuffer.Get(), 1, &copyRegion);
    });


    vkn::AllocationInfo texAllocInfo = {};
    texAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    texAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::ImageCreateInfo texCreateInfo = {};

    texCreateInfo.pDevice = &s_vkDevice;
    texCreateInfo.type = VK_IMAGE_TYPE_2D;
    texCreateInfo.extent.width = 1;
    texCreateInfo.extent.height = 1;
    texCreateInfo.extent.depth = 1;
    texCreateInfo.format = VK_FORMAT_R8_UNORM;
    texCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    texCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    texCreateInfo.mipLevels = 1;
    texCreateInfo.arrayLayers = 1;
    texCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    texCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    texCreateInfo.pAllocInfo = &texAllocInfo;

    s_sceneDefaultImage.Create(texCreateInfo);
    CORE_ASSERT(s_sceneDefaultImage.IsCreated());
    s_sceneDefaultImage.SetDebugName("DEFAULT_TEX");

    vkn::ImageViewCreateInfo texViewCreateInfo = {};

    texViewCreateInfo.pOwner = &s_sceneDefaultImage;
    texViewCreateInfo.type = VK_IMAGE_VIEW_TYPE_2D;
    texViewCreateInfo.format = texCreateInfo.format;
    texViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    texViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    texViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    texViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    texViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    texViewCreateInfo.subresourceRange.baseMipLevel = 0;
    texViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    texViewCreateInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    texViewCreateInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    s_sceneDefaultImageView.Create(texViewCreateInfo);
    CORE_ASSERT(s_sceneDefaultImageView.IsCreated());
    s_sceneDefaultImageView.SetDebugName("DEFAULT_TEX_VIEW");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
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

    s_sceneImages.resize(s_cpuTexturesData.size());
    s_sceneImageViews.resize(s_cpuTexturesData.size());

    for (size_t i = 0; i < s_cpuTexturesData.size(); i += 2) {
        for (size_t j = 0; j < 2; ++j) {
            const size_t index = i + j;

            const TextureLoadData& cpuTexData = s_cpuTexturesData[index];
            texCreateInfo.extent.width = cpuTexData.GetWidth();
            texCreateInfo.extent.height = cpuTexData.GetHeight();
            texCreateInfo.extent.depth = 1;
            texCreateInfo.format = cpuTexData.format;

            s_sceneImages[index].Create(texCreateInfo);
            CORE_ASSERT(s_sceneImages[index].IsCreated());
        #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
            s_sceneImages[index].SetDebugName(cpuTexData.name.data());
        #endif

            vkn::Buffer& texStagingBuffer = s_commonStagingBuffers[j];

            void* pTexData = texStagingBuffer.Map(0, VK_WHOLE_SIZE);

            const size_t texDataSize = cpuTexData.GetTexSize();
            CORE_ASSERT(texDataSize <= texStagingBuffer.GetMemorySize());

            memcpy(pTexData, cpuTexData.GetData(), texDataSize);

            texStagingBuffer.Unmap();
        }

        ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
            for (size_t j = 0; j < 2; ++j) {
                const size_t index = i + j;

                CmdPipelineImageBarrier(
                    cmdBuffer,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_2_NONE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_NONE,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    s_sceneImages[index].Get(),
                    VK_IMAGE_ASPECT_COLOR_BIT
                );

                VkCopyBufferToImageInfo2 copyInfo = {};

                copyInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
                copyInfo.srcBuffer = s_commonStagingBuffers[j].Get();
                copyInfo.dstImage = s_sceneImages[index].Get();
                copyInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                copyInfo.regionCount = 1;

                VkBufferImageCopy2 texRegion = {};

                texRegion.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
                texRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                texRegion.imageSubresource.mipLevel = 0;
                texRegion.imageSubresource.baseArrayLayer = 0;
                texRegion.imageSubresource.layerCount = 1;
                texRegion.imageExtent = s_sceneImages[index].GetSize();

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
                    s_sceneImages[index].Get(),
                    VK_IMAGE_ASPECT_COLOR_BIT
                );
            }
        });
    }
}


static void UploadGPUTransformData()
{
    vkn::Buffer& trsDataStagingBuf = s_commonStagingBuffers[0];

    COMMON_TRANSFORM* pTrsData = trsDataStagingBuf.Map<COMMON_TRANSFORM>();

    const size_t trsDataBufSize = s_cpuTransforms.size() * sizeof(COMMON_TRANSFORM);
    CORE_ASSERT(trsDataBufSize <= trsDataStagingBuf.GetMemorySize());

    for (size_t i = 0; i < s_cpuTransforms.size(); ++i) {
        for (size_t j = 0; j < _countof(COMMON_TRANSFORM::MATR); ++j) {
            pTrsData[i].MATR[j] = s_cpuTransforms[i][j];
        }
    }

    trsDataStagingBuf.Unmap();

    vkn::AllocationInfo bufAllocInfo = {};
    bufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    bufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo bufCreateInfo = {};
    bufCreateInfo.pDevice = &s_vkDevice;
    bufCreateInfo.size = trsDataBufSize;
    bufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufCreateInfo.pAllocInfo = &bufAllocInfo;

    s_commonTransformsBuffer.Create(bufCreateInfo);
    CORE_ASSERT(s_commonTransformsBuffer.IsCreated());
    s_commonTransformsBuffer.SetDebugName("COMMON_TRS_DATA");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        VkBufferCopy bufferRegion = {};
        
        bufferRegion.size = trsDataBufSize;
        vkCmdCopyBuffer(cmdBuffer.Get(), trsDataStagingBuf.Get(), s_commonTransformsBuffer.Get(), 1, &bufferRegion);
    });
}


static void UploadGPUInstData()
{
    vkn::Buffer& instDataStagingBuf = s_commonStagingBuffers[0];

    COMMON_INST_INFO* pInstData = instDataStagingBuf.Map<COMMON_INST_INFO>();

    const size_t instDataBufSize = s_cpuDIPs.size() * sizeof(COMMON_INST_INFO);
    CORE_ASSERT(instDataBufSize <= instDataStagingBuf.GetMemorySize());

    auto FindTrsIndex = [](size_t dipIdx)
    {
        for (size_t nodeIdx = 0; nodeIdx < s_cpuSceneNodes.size(); ++nodeIdx) {
            const SceneNode& node = s_cpuSceneNodes[nodeIdx];

            if (node.dipGroupIdx >= 0) {
                const DIPGroup& dipGroup = s_cpuDIPGroups[node.dipGroupIdx];

                if (dipIdx >= dipGroup.firstDIP && dipIdx < dipGroup.firstDIP + dipGroup.dipCount) {
                    return node.trsIdx;
                }
            }
        }

        return -1;
    };

    for (size_t i = 0; i < s_cpuDIPs.size(); ++i) {
        pInstData[i].TRANSFORM_IDX = FindTrsIndex(i);
        CORE_ASSERT(pInstData[i].TRANSFORM_IDX >= 0);

        pInstData[i].MESH_IDX = s_cpuDIPs[i].meshIdx;
        pInstData[i].MATERIAL_IDX = s_cpuDIPs[i].mtlIdx;
    }

    instDataStagingBuf.Unmap();

    vkn::AllocationInfo bufAllocInfo = {};
    bufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    bufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo bufCreateInfo = {};
    bufCreateInfo.pDevice = &s_vkDevice;
    bufCreateInfo.size = instDataBufSize;
    bufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufCreateInfo.pAllocInfo = &bufAllocInfo;

    s_commonInstDataBuffer.Create(bufCreateInfo);
    CORE_ASSERT(s_commonInstDataBuffer.IsCreated());
    s_commonInstDataBuffer.SetDebugName("COMMON_INST_DATA");
    
    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        VkBufferCopy copyRegion = {};

        copyRegion.size = instDataBufSize;
        vkCmdCopyBuffer(cmdBuffer.Get(), instDataStagingBuf.Get(), s_commonInstDataBuffer.Get(), 1, &copyRegion);
    });
}


static void UploadGPUResources()
{
    UploadGPUMeshData();
    UploadGPUMaterialData();
    UploadGPUTransformData();
    UploadGPUInstData();
}


static void LoadScene(const fs::path& filepath)
{
    const std::string strPath = filepath.string();

    if (!fs::exists(filepath)) {
		CORE_ASSERT_FAIL("Unknown scene path: %s", strPath.c_str());
		return;
	}

    ENG_PROFILE_SCOPED_MARKER_C("Load_Scene", 255, 50, 255, 255);
    
    CORE_LOG_TRACE("Loading \"%s\"...", strPath.c_str());

    Timer timer;

    static constexpr gltf::Extensions requiredExtensions =
        gltf::Extensions::KHR_mesh_quantization |
        gltf::Extensions::KHR_texture_transform |
        gltf::Extensions::KHR_materials_variants;

    gltf::Parser parser(requiredExtensions);

    constexpr gltf::Options options =
        gltf::Options::DontRequireValidAssetMember |
        gltf::Options::LoadExternalBuffers |
        gltf::Options::GenerateMeshIndices;

    gltf::Expected<gltf::MappedGltfFile> gltfFile = gltf::MappedGltfFile::FromPath(filepath);
    if (!gltfFile) {
        CORE_ASSERT_FAIL("Failed to open glTF file: %s", gltf::getErrorMessage(gltfFile.error()).data());
        return;
    }

    gltf::Expected<gltf::Asset> asset = parser.loadGltf(gltfFile.get(), filepath.parent_path(), options);
    if (asset.error() != gltf::Error::None) {
        CORE_ASSERT_FAIL("Failed to load glTF: : %s", gltf::getErrorMessage(asset.error()).data());
        return;
    }

    
    LoadSceneMeshData(asset.get());
    LoadSceneNodes(asset.get());

    LoadSceneTexturesData(asset.get(), filepath.parent_path());
    LoadSceneMaterials(asset.get());
    
    UploadGPUResources();

    s_cpuTexturesData = {};

    CORE_LOG_INFO("FastGLTF: \"%s\" loading finished: %f ms", strPath.c_str(), timer.End().GetDuration<float, std::milli>());
}
#endif


void UpdateCommonConstBuffer()
{
    ENG_PROFILE_SCOPED_MARKER_C("Update_Common_Const_Buffer", 255, 255, 50, 255);

    COMMON_CB_DATA* pCommonConstBufferData = s_commonConstBuffer.Map<COMMON_CB_DATA>();

    pCommonConstBufferData->COMMON_VIEW_MATRIX = s_camera.GetViewMatrix();
    pCommonConstBufferData->COMMON_PROJ_MATRIX = s_camera.GetProjMatrix();
    pCommonConstBufferData->COMMON_VIEW_PROJ_MATRIX = s_camera.GetViewProjMatrix();

    memcpy(&pCommonConstBufferData->COMMON_CAMERA_FRUSTUM, &s_camera.GetFrustum(), sizeof(FRUSTUM));
    
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

#ifndef ENG_BUILD_RELEASE
    dbgFlags |= s_useMeshIndirectDraw ? (uint32_t)COMMON_DBG_FLAG_MASKS::USE_MESH_INDIRECT_DRAW : 0;
    dbgFlags |= s_useMeshCulling ? (uint32_t)COMMON_DBG_FLAG_MASKS::USE_MESH_GPU_CULLING : 0;
#endif

    pCommonConstBufferData->COMMON_DBG_FLAGS = dbgFlags;

    s_commonConstBuffer.Unmap();
}


void UpdateScene()
{
    DbgUI::BeginFrame();

    const float moveDist = glm::length(s_cameraVel);

    if (!math::IsZero(moveDist)) {
        const glm::vec3 moveDir = glm::normalize(s_camera.GetRotation() * (s_cameraVel / moveDist));
        s_camera.MoveAlongDir(moveDir, moveDist);
    }

    s_camera.Update();
}


void PresentImage(uint32_t imageIndex)
{
    ENG_PROFILE_SCOPED_MARKER_C("Present_Swapchain_Image", 50, 50, 255, 255);
    
    VkSwapchainKHR vkSwapchain = s_vkSwapchain.Get();
    VkSemaphore vkWaitSemaphore = s_renderFinishedSemaphores[imageIndex].Get();

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &vkWaitSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &vkSwapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;
    const VkResult presentResult = vkQueuePresentKHR(s_vkDevice.GetQueue(), &presentInfo);

    if (presentResult != VK_SUBOPTIMAL_KHR && presentResult != VK_ERROR_OUT_OF_DATE_KHR) {
        VK_CHECK(presentResult);
    } else {
        s_swapchainRecreateRequired = true;
    }
}


void BaseCullingPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "BaseMesh_Culling_Pass", 50, 50, 255, 255);

    vkCmdBindPipeline(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE, s_baseCullingPipeline);
    
    vkCmdBindDescriptorSets(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE, s_baseCullingPipelineLayout, 0, 1, &s_commonDescriptorSet, 0, nullptr);

    BASE_CULLING_BINDLESS_REGISTRY registry = {};
    registry.INST_COUNT = s_cpuDIPs.size();

    vkCmdPushConstants(cmdBuffer.Get(), s_baseCullingPipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(BASE_CULLING_BINDLESS_REGISTRY), &registry);

    vkCmdDispatch(cmdBuffer.Get(), (s_cpuDIPs.size() + 63) / 64, 1, 1);

    CmdPipelineBufferBarrier(
        cmdBuffer, 
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT,
        s_drawIndirectCommandsBuffer.Get()
    );

    CmdPipelineBufferBarrier(
        cmdBuffer, 
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT,
        s_drawIndirectCommandsCountBuffer.Get()
    );
}


static bool IsInstVisible(const COMMON_INST_INFO& instInfo)
{
    ENG_PROFILE_SCOPED_MARKER_C("CPU_Is_Inst_Visible", 50, 200, 50, 255);

    // const COMMON_MESH_INFO& mesh = s_sceneMeshInfos[instInfo.MESH_IDX];
    
    // glm::vec3 aabbMin = mesh.BOUNDS_MIN_LCS;
    // glm::vec3 aabbMax = mesh.BOUNDS_MAX_LCS;

    // const COMMON_TRANSFORM& trs = s_sceneTransforms[instInfo.TRANSFORM_IDX];
    // const glm::mat3x4 wMatr = glm::mat3x4(trs.MATR[0], trs.MATR[1], trs.MATR[2]);

    // const glm::vec3 newMin(glm::vec4(aabbMin, 1.f) * wMatr);
    // const glm::vec3 newMax(glm::vec4(aabbMax, 1.f) * wMatr);

    // aabbMin = glm::min(newMin, newMax);
    // aabbMax = glm::max(newMin, newMax);

    // const math::Frustum& frustum = s_camera.GetFrustum();

    // float minDot = FLT_MAX, maxDot = -FLT_MAX;

    // for (size_t i = 0; i < COMMON_FRUSTUM_PLANES_COUNT; ++i) {
    //     minDot = glm::dot(aabbMin, frustum.planes[i].normal) + frustum.planes[i].distance;
    //     maxDot = glm::dot(aabbMax, frustum.planes[i].normal) + frustum.planes[i].distance;

    //     if (minDot < 0.f && maxDot < 0.f) {
    //         return false;
    //     }
    // }

    return true;
}


void BaseRenderPass(vkn::CmdBuffer& cmdBuffer, const VkExtent2D& extent)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "BaseMesh_Render_Pass", 128, 128, 128, 255);

    VkViewport viewport = {};
    viewport.width = extent.width;
    viewport.height = extent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    cmdBuffer.CmdSetViewport(0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.extent = extent;
    cmdBuffer.CmdSetScissor(0, 1, &scissor);

    vkCmdBindPipeline(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, s_basePipeline);
    
    vkCmdBindDescriptorSets(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, s_basePipelineLayout, 0, 1, &s_commonDescriptorSet, 0, nullptr);

    cmdBuffer.CmdBindIndexBuffer(s_indexBuffer, 0, GetVkIndexType());

    BASE_BINDLESS_REGISTRY registry = {};

#ifndef ENG_BUILD_RELEASE
    if (!s_useMeshIndirectDraw) {
        ENG_PROFILE_SCOPED_MARKER_C("CPU_Frustum_Culling", 50, 255, 50, 255);

        s_dbgDrawnMeshCount = 0;

    //     for (uint32_t i = 0; i < s_sceneInstInfos.size(); ++i) {
    //         if (s_useMeshCulling) {
    //             if (!IsInstVisible(s_sceneInstInfos[i])) {
    //                 continue;
    //             }
    //         }

    //         ++s_dbgDrawnMeshCount;

    //         registry.INST_INFO_IDX = i;
    //         vkCmdPushConstants(cmdBuffer.Get(), s_basePipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(BASE_BINDLESS_REGISTRY), &registry);

    //         // const COMMON_MESH_INFO& mesh = s_sceneMeshInfos[s_sceneInstInfos[i].MESH_IDX];
    //         // cmdBuffer.CmdDrawIndexed(mesh.INDEX_COUNT, 1, mesh.FIRST_INDEX, mesh.FIRST_VERTEX, i);
    //     }
    } else 
#endif
    {
        vkCmdPushConstants(cmdBuffer.Get(), s_basePipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(BASE_BINDLESS_REGISTRY), &registry);

        cmdBuffer.CmdDrawIndexedIndirect(s_drawIndirectCommandsBuffer, 0, s_drawIndirectCommandsCountBuffer, 0, MAX_INDIRECT_DRAW_CMD_COUNT, sizeof(BASE_INDIRECT_DRAW_CMD));
    }
}


void RenderScene()
{
    ENG_PROFILE_SCOPED_MARKER_C("Render_Scene", 255, 255, 50, 255);

    vkn::Fence& renderingFinishedFence = s_renderFinishedFence;

    const VkResult fenceStatus = vkGetFenceStatus(s_vkDevice.Get(), renderingFinishedFence.Get());
    if (fenceStatus == VK_NOT_READY) {
        DbgUI::EndFrame();
        return;
    } else {
        renderingFinishedFence.Reset();
    }

    UpdateCommonConstBuffer();

    vkn::Semaphore& presentFinishedSemaphore = s_presentFinishedSemaphore;

    uint32_t nextImageIdx;
    const VkResult acquireResult = vkAcquireNextImageKHR(s_vkDevice.Get(), s_vkSwapchain.Get(), 10'000'000'000, presentFinishedSemaphore.Get(), VK_NULL_HANDLE, &nextImageIdx);
    
    if (acquireResult != VK_SUBOPTIMAL_KHR && acquireResult != VK_ERROR_OUT_OF_DATE_KHR) {
        VK_CHECK(acquireResult);
    } else {
        s_swapchainRecreateRequired = true;
        DbgUI::EndFrame();
        return;
    }

    vkn::Semaphore& renderingFinishedSemaphore = s_renderFinishedSemaphores[nextImageIdx];
    vkn::CmdBuffer& cmdBuffer = s_renderCmdBuffer;

    cmdBuffer.Reset();

    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkImage rndImage = s_vkSwapchain.GetImage(nextImageIdx);

    cmdBuffer.Begin(cmdBeginInfo);
    {
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "CMD_Buffer_Frame", 255, 165, 0, 255);

        BaseCullingPass(cmdBuffer);

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
            s_depthRT.Get(),
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
        depthAttachment.imageView = s_depthRTView.Get();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    #ifdef ENG_REVERSED_Z
        depthAttachment.clearValue.depthStencil.depth = 0.f;
    #else
        depthAttachment.clearValue.depthStencil.depth = 1.f;
    #endif

        renderingInfo.pDepthAttachment = &depthAttachment;
        
        cmdBuffer.CmdBeginRendering(renderingInfo);
            BaseRenderPass(cmdBuffer, renderingInfo.renderArea.extent);

            DbgUI::FillData();
            DbgUI::EndFrame();
            
            DbgUI::Render(cmdBuffer);
        cmdBuffer.CmdEndRendering();

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

        ENG_PROFILE_GPU_COLLECT_STATS(cmdBuffer);
    }
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

    PresentImage(nextImageIdx);
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
            const float finalSpeed = CAMERA_SPEED * s_frameTime;

            if (keyEvent.key == WndKey::KEY_W) { 
                s_cameraVel.z = -finalSpeed;
            }
            if (keyEvent.key == WndKey::KEY_S) {
                s_cameraVel.z = finalSpeed;
            }
            if (keyEvent.key == WndKey::KEY_A) {
                s_cameraVel.x = -finalSpeed;
            }
            if (keyEvent.key == WndKey::KEY_D) {
                s_cameraVel.x = finalSpeed;
            }
            if (keyEvent.key == WndKey::KEY_E) {
                s_cameraVel.y = finalSpeed;
            }
            if (keyEvent.key == WndKey::KEY_Q) {
                s_cameraVel.y = -finalSpeed;
            }
            if (keyEvent.key == WndKey::KEY_F5) {
                firstEvent = true;
            }
        }

        if (keyEvent.IsReleased()) {
            if (keyEvent.key == WndKey::KEY_W) {
                s_cameraVel.z = 0;
            }
            if (keyEvent.key == WndKey::KEY_S) {
                s_cameraVel.z = 0;
            }
            if (keyEvent.key == WndKey::KEY_A) {
                s_cameraVel.x = 0;
            }
            if (keyEvent.key == WndKey::KEY_D) {
                s_cameraVel.x = 0;
            }
            if (keyEvent.key == WndKey::KEY_E) {
                s_cameraVel.y = 0;
            }
            if (keyEvent.key == WndKey::KEY_Q) {
                s_cameraVel.y = 0;
            }
        }
    } else if (event.Is<WndCursorEvent>()) {
        CORE_ASSERT(s_pWnd->IsCursorRelativeMode());

        static glm::vec3 pitchYawRoll = s_camera.GetPitchYawRollDegrees();

        if (firstEvent) {
            firstEvent = false;
        } else {
            const float yaw = s_pWnd->GetCursorDX() / 5.f;
            const float pitch = s_pWnd->GetCursorDY() / 5.f;
            
            pitchYawRoll.x -= pitch;
            pitchYawRoll.y -= yaw;
            
            pitchYawRoll.x = (pitchYawRoll.x > 89.0f) ? 89.0f : pitchYawRoll.x;
            pitchYawRoll.x = (pitchYawRoll.x < -89.0f) ? -89.0f : pitchYawRoll.x;

            pitchYawRoll.z = 0.f;

            glm::vec3 cameraDir;
            cameraDir.x = -glm::cos(glm::radians(pitchYawRoll.x)) * glm::sin(glm::radians(pitchYawRoll.y));
            cameraDir.y =  glm::sin(glm::radians(pitchYawRoll.x));
            cameraDir.z = -glm::cos(glm::radians(pitchYawRoll.x)) * glm::cos(glm::radians(pitchYawRoll.y));
            cameraDir = glm::normalize(cameraDir);

            const glm::vec3 cameraRight = glm::normalize(glm::cross(cameraDir, M3D_AXIS_Y));
			const glm::vec3 cameraUp    = glm::cross(cameraRight, cameraDir);
            const glm::quat newRotation = glm::quatLookAt(cameraDir, cameraUp);

            camera.SetRotation(newRotation);
        }
    } else if (event.Is<WndResizeEvent>()) {
        const WndResizeEvent& resizeEvent = event.Get<WndResizeEvent>();

        if (!resizeEvent.IsMinimized() && resizeEvent.height != 0) {
            camera.SetAspectRatio((float)resizeEvent.width / (float)resizeEvent.height);
        }
    }
}


void ProcessWndEvent(const WndEvent& event)
{
    if (event.Is<WndResizeEvent>()) {
        s_swapchainRecreateRequired = true;
    }

    if (DbgUI::IsAnyWindowFocused()) {
        return;
    }

    if (event.Is<WndKeyEvent>()) {
        const WndKeyEvent& keyEvent = event.Get<WndKeyEvent>();

        if (keyEvent.key == WndKey::KEY_F5 && keyEvent.IsPressed()) {
            s_flyCameraMode = !s_flyCameraMode;
            s_pWnd->SetCursorRelativeMode(s_flyCameraMode);
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

    static Timer timer;
    timer.End().GetDuration<float, std::milli>(s_frameTime).Reset();

    s_pWnd->ProcessEvents();
    
    WndEvent event;
    while(s_pWnd->PopEvent(event)) {
        ProcessWndEvent(event);
    }

    if (s_pWnd->IsMinimized()) {
        return;
    }

    if (s_swapchainRecreateRequired) {
        if (ResizeVkSwapchain(s_pWnd)) {
            return;
        }

        s_vkDevice.WaitIdle();
        CreateDepthRT();
    }

    UpdateScene();
    RenderScene();

    ++s_frameNumber;

    ENG_PROFILE_END_FRAME("Frame");
}


int main(int argc, char* argv[])
{
#ifdef FREEIMAGE_LIB
    FreeImage_Initialise();
#endif

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

    vkn::SurfaceCreateInfo vkSurfCI = {};
    vkSurfCI.pInstance = &s_vkInstance;
    vkSurfCI.pWndHandle = s_pWnd->GetNativeHandle();

    s_vkSurface.Create(vkSurfCI);
    CORE_ASSERT(s_vkSurface.IsCreated());

    CreateVkPhysAndLogicalDevices();

#ifdef ENG_PROFILING_ENABLED
    vkn::GetProfiler().Create(&s_vkDevice);
    CORE_ASSERT(vkn::GetProfiler().IsCreated());
#endif

    vkn::AllocatorCreateInfo vkAllocatorCI = {}; 
    vkAllocatorCI.pDevice = &s_vkDevice;
    // RenderDoc doesn't work with buffer device address if you use VMA :(
    // vkAllocatorCI.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    s_vkAllocator.Create(vkAllocatorCI);
    CORE_ASSERT(s_vkAllocator.IsCreated());

    CreateVkSwapchain();

    DbgUI::Init();

    vkn::CmdPoolCreateInfo vkCmdPoolCI = {};
    vkCmdPoolCI.pDevice = &s_vkDevice;
    vkCmdPoolCI.queueFamilyIndex = s_vkDevice.GetQueueFamilyIndex();
    vkCmdPoolCI.flags =  VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    s_cmdPool.Create(vkCmdPoolCI);
    CORE_ASSERT(s_cmdPool.IsCreated());
    s_cmdPool.SetDebugName("COMMON_CMD_POOL");
    
    s_immediateSubmitCmdBuffer = s_cmdPool.AllocCmdBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    CORE_ASSERT(s_immediateSubmitCmdBuffer.IsCreated());
    s_immediateSubmitCmdBuffer.SetDebugName("IMMEDIATE_CMD_BUFFER");

    s_immediateSubmitFinishedFence.Create(&s_vkDevice);

    vkn::QueryCreateInfo queryCI = {};
    queryCI.pDevice = &s_vkDevice;
    queryCI.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryCI.queryCount = 128;

    s_queryPool.Create(queryCI);
    CORE_ASSERT(s_queryPool.IsCreated());
    s_queryPool.SetDebugName("COMMON_GPU_QUERY_POOL");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        cmdBuffer.CmdResetQueryPool(s_queryPool);
    });

    CreateCommonStagingBuffers();

    s_commonDescriptorPool = CreateVkCommonDescriptorPool(s_vkDevice.Get());
    s_commonDescriptorSetLayout = CreateVkCommonDescriptorSetLayout(s_vkDevice.Get());
    s_commonDescriptorSet = CreateVkCommonDescriptorSet(s_vkDevice.Get(), s_commonDescriptorPool, s_commonDescriptorSetLayout);

    s_basePipelineLayout = CreateVkBasePipelineLayout(s_vkDevice.Get(), s_commonDescriptorSetLayout);
    s_basePipeline = CreateVkBasePipeline(s_vkDevice.Get(), s_basePipelineLayout, "shaders/bin/base.vs.spv", "shaders/bin/base.ps.spv");

    s_baseCullingPipelineLayout = CreateVkBaseCullingPipelineLayout(s_vkDevice.Get(), s_commonDescriptorSetLayout);
    s_baseCullingPipeline = CreateVkBaseCullingPipeline(s_vkDevice.Get(), s_basePipelineLayout, "shaders/bin/base_culling.cs.spv");

    const size_t swapchainImageCount = s_vkSwapchain.GetImageCount();

    s_renderFinishedSemaphores.resize(swapchainImageCount);
    for (size_t i = 0; i < swapchainImageCount; ++i) {
        s_renderFinishedSemaphores[i].Create(&s_vkDevice);
        CORE_ASSERT(s_renderFinishedSemaphores[i].IsCreated());

        s_renderFinishedSemaphores[i].SetDebugName("RND_FINISH_SEMAPHORE_%zu", i);
    }

    s_presentFinishedSemaphore.Create(&s_vkDevice);
    CORE_ASSERT(s_presentFinishedSemaphore.IsCreated());
    s_presentFinishedSemaphore.SetDebugName("PRESENT_FINISH_SEMAPHORE");

    s_renderFinishedFence.Create(&s_vkDevice);
    CORE_ASSERT(s_renderFinishedFence.IsCreated());
    s_renderFinishedFence.SetDebugName("RND_FINISH_FENCE");
    
    s_renderCmdBuffer = s_cmdPool.AllocCmdBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    CORE_ASSERT(s_renderCmdBuffer.IsCreated());
    s_renderCmdBuffer.SetDebugName("RND_CMD_BUFFER");

    CreateDepthRT();
    CreateCommonSamplers();

    LoadScene(argc > 1 ? argv[1] : "../assets/Sponza/Sponza.gltf");

    CreateVkIndirectDrawBuffers();

    vkn::AllocationInfo commonConstBufAI = {};
    commonConstBufAI.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    commonConstBufAI.usage = VMA_MEMORY_USAGE_AUTO;
    
    vkn::BufferCreateInfo commonConstBufCI = {};
    commonConstBufCI.pDevice = &s_vkDevice;
    commonConstBufCI.size = sizeof(COMMON_CB_DATA);
    commonConstBufCI.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    commonConstBufCI.pAllocInfo = &commonConstBufAI;

    s_commonConstBuffer.Create(commonConstBufCI); 
    CORE_ASSERT(s_commonConstBuffer.IsCreated());
    s_commonConstBuffer.SetDebugName("COMMON_CB");

    WriteDescriptorSet();

    s_camera.SetPosition(glm::vec3(0.f, 2.f, 0.f));
    s_camera.SetRotation(glm::quatLookAt(M3D_AXIS_X, M3D_AXIS_Y));
    s_camera.SetPerspProjection(glm::radians(90.f), (float)s_pWnd->GetWidth() / s_pWnd->GetHeight(), 0.01f, 100'000.f);

    s_pWnd->SetVisible(true);

    while(!s_pWnd->IsClosed()) {
        ProcessFrame();
    }

    s_vkDevice.WaitIdle();

    
    vkDestroyPipeline(s_vkDevice.Get(), s_baseCullingPipeline, nullptr);
    vkDestroyPipelineLayout(s_vkDevice.Get(), s_baseCullingPipelineLayout, nullptr);
    vkDestroyPipeline(s_vkDevice.Get(), s_basePipeline, nullptr);
    vkDestroyPipelineLayout(s_vkDevice.Get(), s_basePipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_commonDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(s_vkDevice.Get(), s_commonDescriptorPool, nullptr);

    DbgUI::Terminate();

    s_pWnd->Destroy();

    wndSysTerminate();

#ifdef FREEIMAGE_LIB
    FreeImage_DeInitialise();
#endif

    return 0;
}