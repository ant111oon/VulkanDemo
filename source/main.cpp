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
#include "render/core/vulkan/vk_texture.h"
#include "render/core/vulkan/vk_pipeline.h"
#include "render/core/vulkan/vk_query.h"

#include "render/core/vulkan/vk_memory.h"

#include "core/engine/camera/camera.h"

#include "core/profiler/cpu_profiler.h"
#include "render/core/vulkan/vk_profiler.h"

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_win32.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>


namespace gltf  = fastgltf;
namespace fs    = std::filesystem;
using IndexType = uint32_t;


class TextureLoadData
{
public:
    enum class ComponentType
    {
        UINT8,
        UINT16,
        FLOAT,
    };

public:
    TextureLoadData() = default;

    ~TextureLoadData()
    {
        Unload();
    }

    TextureLoadData(const TextureLoadData& other) = delete;
    TextureLoadData& operator=(const TextureLoadData& other) = delete;

    TextureLoadData(TextureLoadData&& other) noexcept
    {
        *this = std::move(other);
    }

    TextureLoadData& operator=(TextureLoadData&& other)
    {
    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        m_name = std::move(other.m_name);
    #endif

        m_pData = other.m_pData;
        other.m_pData = nullptr;

        m_format = other.m_format;
        other.m_format = VK_FORMAT_UNDEFINED;

        m_width = other.m_width;
        m_height = other.m_height;
        m_channels = other.m_channels;
        m_type = other.m_type;

        other.m_width = 0;
        other.m_height = 0;
        other.m_channels = 0;
        other.m_type = ComponentType::UINT8;

        return *this;
    }

    bool Load(const fs::path& filepath)
    {
        if (IsLoaded()) {
            Unload();
        }

        const std::string strPath = filepath.string();

        int width = 0;
        int height = 0;
        int channels = 0;

        const int result = stbi_info(strPath.c_str(), &width, &height, &channels);
        CORE_ASSERT(result == 1);

        const bool isRGB = channels == 3;

        if (stbi_is_16_bit(strPath.c_str())) {
            m_pData = stbi_load_16(strPath.c_str(), &width, &height, &channels, isRGB ? 4 : 0);
            m_type = ComponentType::UINT16;
        } else if (stbi_is_hdr(strPath.c_str())) {
            m_pData = stbi_loadf(strPath.c_str(), &width, &height, &channels, isRGB ? 4 : 0);
            m_type = ComponentType::FLOAT;
        } else {
            m_pData = stbi_load(strPath.c_str(), &width, &height, &channels, isRGB ? 4 : 0);
            m_type = ComponentType::UINT8;
        }

        if (!m_pData) {
            return false;
        }

        channels = isRGB ? 4 : channels;

        m_width = width;
        m_height = height;
        m_channels = channels;

        m_format = EvaluateFormat(m_channels, m_type);

        return true;
    }

    bool Load(const void* pMemory, size_t size)
    {
        if (IsLoaded()) {
            Unload();
        }

        CORE_ASSERT(pMemory != nullptr);
        CORE_ASSERT(size > 0);

        int width = 0;
        int height = 0;
        int channels = 0;

        if (stbi_is_16_bit_from_memory((const stbi_uc*)pMemory, size)) {
            m_pData = stbi_load_16_from_memory((const stbi_uc*)pMemory, size, &width, &height, &channels, 0);
            m_type = ComponentType::UINT16;
        } else if (stbi_is_hdr_from_memory((const stbi_uc*)pMemory, size)) {
            m_pData = stbi_loadf_from_memory((const stbi_uc*)pMemory, size, &width, &height, &channels, 0);
            m_type = ComponentType::FLOAT;
        } else {
            m_pData = stbi_load_from_memory((const stbi_uc*)pMemory, size, &width, &height, &channels, 0);
            m_type = ComponentType::UINT8;
        }

        if (!m_pData) {
            return false;
        }

        m_width = width;
        m_height = height;
        m_channels = channels;

        m_format = EvaluateFormat(m_channels, m_type);

        return true;
    }

    void Unload()
    {
        if (!IsLoaded()) {
            return;
        }

    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        m_name = "";
    #endif

        stbi_image_free(m_pData);
        m_pData = nullptr;

        m_format = VK_FORMAT_UNDEFINED;

        m_width = 0;
        m_height = 0;
        m_channels = 0;
        m_type = ComponentType::UINT8;
    }
    
    void SetName(std::string_view name)
    {
    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        m_name = name;
    #endif
    }

    const char* GetName() const
    {
    #ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
        return m_name.c_str();
    #else
        return "TEXTURE";
    #endif
    }

    void* GetData() const { return m_pData; }
    VkFormat GetFormat() const { return m_format; }

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    uint32_t GetChannels() const { return m_channels; }

    ComponentType GetComponentType() const { return m_type; }
    
    size_t GetMemorySize() const { return m_width * m_height * m_channels * COMP_TYPE_SIZE_IN_BYTES[static_cast<size_t>(m_type)]; }

    bool IsLoaded() const { return m_pData != nullptr; }

private:
    static VkFormat EvaluateFormat(uint32_t channels, ComponentType type)
    {
        switch (channels) {
            case 1:
                switch (type) {
                    case ComponentType::UINT8:  return VK_FORMAT_R8_UNORM;
                    case ComponentType::UINT16: return VK_FORMAT_R16_UNORM;
                    case ComponentType::FLOAT:  return VK_FORMAT_R32_SFLOAT;
                    default:
                        CORE_ASSERT_FAIL("Invalid texture component type: %u", static_cast<uint32_t>(type));
                        return VK_FORMAT_UNDEFINED;
                }
                break;
            case 2:
                switch (type) {
                    case ComponentType::UINT8:  return VK_FORMAT_R8G8_UNORM;
                    case ComponentType::UINT16: return VK_FORMAT_R16G16_UNORM;
                    case ComponentType::FLOAT:  return VK_FORMAT_R32G32_SFLOAT;
                    default:
                        CORE_ASSERT_FAIL("Invalid texture component type: %u", static_cast<uint32_t>(type));
                        return VK_FORMAT_UNDEFINED;
                }
                break;
            case 3:
                switch (type) {
                    case ComponentType::UINT8:  return VK_FORMAT_R8G8B8_UNORM;
                    case ComponentType::UINT16: return VK_FORMAT_R16G16B16_UNORM;
                    case ComponentType::FLOAT:  return VK_FORMAT_R32G32B32_SFLOAT;
                    default:
                        CORE_ASSERT_FAIL("Invalid texture component type: %u", static_cast<uint32_t>(type));
                        return VK_FORMAT_UNDEFINED;
                }
                break;
            case 4:
                switch (type) {
                    case ComponentType::UINT8:  return VK_FORMAT_R8G8B8A8_UNORM;
                    case ComponentType::UINT16: return VK_FORMAT_R16G16B16A16_UNORM;
                    case ComponentType::FLOAT:  return VK_FORMAT_R32G32B32A32_SFLOAT;
                    default:
                        CORE_ASSERT_FAIL("Invalid texture component type: %u", static_cast<uint32_t>(type));
                        return VK_FORMAT_UNDEFINED;
                }
                break;
            default:
                CORE_ASSERT_FAIL("Invalid texture channels count: %u", channels);
                return VK_FORMAT_UNDEFINED;
        }
    }
    
private:
    static inline constexpr size_t COMP_TYPE_SIZE_IN_BYTES[] = { 1, 2, 4 };

private:
#ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
    std::string m_name;
#endif

    void* m_pData = nullptr;
    VkFormat m_format = VK_FORMAT_UNDEFINED;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_channels = 0;
    ComponentType m_type = ComponentType::UINT8;
};


static constexpr uint32_t VERTEX_DATA_SIZE_UI = 6;

struct Vertex
{
    void Pack(const glm::float3& lpos, const glm::float3& lnorm, glm::float2 uv, const glm::float4& tangent)
    {
        data[0] = glm::packHalf2x16(glm::float2(lpos.x, lpos.y));
        data[1] = glm::packHalf2x16(glm::float2(lpos.z, lnorm.x));
        data[2] = glm::packHalf2x16(glm::float2(lnorm.y, lnorm.z));
        data[3] = glm::packHalf2x16(uv);
        data[4] = glm::packHalf2x16(glm::float2(tangent.x, tangent.y));
        data[5] = glm::packHalf2x16(glm::float2(tangent.z, tangent.w));
    }

    void Unpack(glm::float3& outLPos, glm::float3& outLNorm, glm::float2& outUv)
    {
        outLPos = GetLPos();
        outLNorm = GetLNorm();
        outUv = GetUV();
    }

    glm::float3 GetLPos() const { return glm::float3(glm::unpackHalf2x16(data[0]), glm::unpackHalf2x16(data[1]).x); }
    glm::float3 GetLNorm() const { return glm::float3(glm::unpackHalf2x16(data[1]).y, glm::unpackHalf2x16(data[2])); }
    glm::float2 GetUV() const { return glm::unpackHalf2x16(data[3]); }

    glm::uint data[VERTEX_DATA_SIZE_UI] = {};
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

    glm::float3 SPHERE_BOUNDS_CENTER_LCS;
    float SPHERE_BOUNDS_RADIUS_LCS;
};


struct COMMON_INST_INFO
{
    glm::uint TRANSFORM_IDX;
    glm::uint MATERIAL_IDX;
    glm::uint MESH_IDX;
    glm::uint PAD0;
};


struct COMMON_INDIRECT_DRAW_CMD
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
    glm::float3 normal;
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
    glm::float4x4 COMMON_VIEW_MATRIX;
    glm::float4x4 COMMON_PROJ_MATRIX;
    glm::float4x4 COMMON_VIEW_PROJ_MATRIX;

    FRUSTUM COMMON_CAMERA_FRUSTUM;

    glm::uint  COMMON_FLAGS;
    glm::uint  COMMON_DBG_FLAGS;
    glm::uint  COMMON_DBG_VIS_FLAGS;
    glm::uint  PAD0;
};


enum COMMON_DBG_FLAG_MASKS
{
    USE_MESH_INDIRECT_DRAW_MASK = 0x1,
    USE_MESH_GPU_CULLING_MASK = 0x2
};


enum COMMON_DBG_VIS_FLAG_MASKS
{
    DBG_VIS_GBUFFER_ALBEDO_MASK = 0x1,
    DBG_VIS_GBUFFER_NORMAL_MASK = 0x2,
    DBG_VIS_GBUFFER_METALNESS_MASK = 0x4,
    DBG_VIS_GBUFFER_ROUGHNESS_MASK = 0x8,
    DBG_VIS_GBUFFER_AO_MASK = 0x10,
    DBG_VIS_GBUFFER_EMISSIVE_MASK = 0x20,
    DBG_VIS_VERT_NORMAL_MASK = 0x40,
    DBG_VIS_VERT_TANGENT_MASK = 0x80,
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


enum class COMMON_DBG_TEX_IDX : glm::uint
{
    RED,
    GREEN,
    BLUE,
    BLACK,
    WHITE,
    GREY,
    CHECKERBOARD,

    COUNT
};


struct MESH_CULLING_BINDLESS_REGISTRY
{
    glm::float3 PAD0;
    glm::uint INST_COUNT;
};


struct ZPASS_BINDLESS_REGISTRY
{
    glm::float3 PAD0;
    glm::uint INST_INFO_IDX;
};


struct GBUFFER_BINDLESS_REGISTRY
{
    glm::float3 PAD0;
    glm::uint INST_INFO_IDX;
};


struct GBuffer
{
    enum
    {
        RT_0,
        RT_1,
        RT_2,
        RT_3,
        RT_COUNT
    };

    std::array<vkn::Texture, RT_COUNT> colorRTs;
    std::array<vkn::TextureView, RT_COUNT> colorRTViews;

    vkn::Texture depthRT;
    vkn::TextureView depthRTView;
};


static constexpr const char* DBG_RT_OUTPUT_NAMES[] = {
    "GBUFFER ALBEDO",
    "GBUFFER NORMAL",
    "GBUFFER METALNESS",
    "GBUFFER ROUGHNESS",
    "GBUFFER AO",
    "GBUFFER EMISSIVE",
    "VERT NORMAL",
    "VERT TANGENT",
};


static constexpr COMMON_DBG_VIS_FLAG_MASKS DBG_RT_OUTPUT_MASKS[] = {
    COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_ALBEDO_MASK,
    COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_NORMAL_MASK,
    COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_METALNESS_MASK,
    COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_ROUGHNESS_MASK,
    COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_AO_MASK,
    COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_EMISSIVE_MASK,
    COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_VERT_NORMAL_MASK,
    COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_VERT_TANGENT_MASK,
};


static_assert(_countof(DBG_RT_OUTPUT_NAMES) == _countof(DBG_RT_OUTPUT_MASKS));


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


static constexpr size_t COMMON_SAMPLERS_DESCRIPTOR_SLOT = 0;
static constexpr size_t COMMON_CONST_BUFFER_DESCRIPTOR_SLOT = 1;
static constexpr size_t COMMON_MESH_INFOS_DESCRIPTOR_SLOT = 2;
static constexpr size_t COMMON_TRANSFORMS_DESCRIPTOR_SLOT = 3;
static constexpr size_t COMMON_MATERIALS_DESCRIPTOR_SLOT = 4;
static constexpr size_t COMMON_MTL_TEXTURES_DESCRIPTOR_SLOT = 5;
static constexpr size_t COMMON_INST_INFOS_DESCRIPTOR_SLOT = 6;
static constexpr size_t COMMON_VERTEX_DATA_DESCRIPTOR_SLOT = 7;
static constexpr size_t COMMON_DBG_TEXTURES_DESCRIPTOR_SLOT = 8;

static constexpr size_t MESH_CULLING_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT = 0;
static constexpr size_t MESH_CULLING_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT = 1;

static constexpr size_t ZPASS_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT = 0;
static constexpr size_t ZPASS_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT = 1;

static constexpr size_t GBUFFER_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT = 0;
static constexpr size_t GBUFFER_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT = 1;

static constexpr uint32_t COMMON_BINDLESS_TEXTURES_COUNT = 128;

static constexpr uint32_t MAX_INDIRECT_DRAW_CMD_COUNT = 1024;

static constexpr size_t STAGING_BUFFER_SIZE    = 96 * 1024 * 1024; // 96 MB
static constexpr size_t STAGING_BUFFER_COUNT   = 2;

static constexpr const char* APP_NAME = "Vulkan Demo";

static constexpr bool VSYNC_ENABLED = false;

static constexpr float CAMERA_SPEED = 0.0025f;


static Window* s_pWnd = nullptr;

static vkn::Instance& s_vkInstance = vkn::GetInstance();
static vkn::Surface& s_vkSurface = vkn::GetSurface();

static vkn::PhysicalDevice& s_vkPhysDevice = vkn::GetPhysicalDevice();
static vkn::Device&         s_vkDevice = vkn::GetDevice();

static vkn::Allocator& s_vkAllocator = vkn::GetAllocator();

static vkn::Swapchain& s_vkSwapchain = vkn::GetSwapchain();

static vkn::CmdPool s_commonCmdPool;

static vkn::CmdBuffer s_immediateSubmitCmdBuffer;
static vkn::Fence s_immediateSubmitFinishedFence;

static std::vector<vkn::Semaphore> s_renderFinishedSemaphores;
static vkn::Semaphore s_presentFinishedSemaphore;
static vkn::Fence     s_renderFinishedFence;
static vkn::CmdBuffer s_renderCmdBuffer;

static std::array<vkn::Buffer, STAGING_BUFFER_COUNT> s_commonStagingBuffers;

static VkDescriptorPool      s_commonDescriptorSetPool = VK_NULL_HANDLE;

static VkDescriptorSet       s_commonDescriptorSet = VK_NULL_HANDLE;
static VkDescriptorSetLayout s_commonDescriptorSetLayout = VK_NULL_HANDLE;

static VkDescriptorSet       s_meshCullingDescriptorSet = VK_NULL_HANDLE;
static VkDescriptorSetLayout s_meshCullingDescriptorSetLayout = VK_NULL_HANDLE;

static VkDescriptorSet       s_zpassDescriptorSet = VK_NULL_HANDLE;
static VkDescriptorSetLayout s_zpassDescriptorSetLayout = VK_NULL_HANDLE;

static VkDescriptorSet       s_gbufferRenderDescriptorSet = VK_NULL_HANDLE;
static VkDescriptorSetLayout s_gbufferRenderDescriptorSetLayout = VK_NULL_HANDLE;

static VkPipelineLayout s_meshCullingPipelineLayout = VK_NULL_HANDLE;
static VkPipeline       s_meshCullingPipeline = VK_NULL_HANDLE;

static VkPipelineLayout s_zpassPipelineLayout = VK_NULL_HANDLE;
static VkPipeline       s_zpassPipeline = VK_NULL_HANDLE;

static VkPipelineLayout s_gbufferRenderPipelineLayout = VK_NULL_HANDLE;
static VkPipeline       s_gbufferRenderPipeline = VK_NULL_HANDLE;

static vkn::Buffer s_vertexBuffer;
static vkn::Buffer s_indexBuffer;

static vkn::Buffer s_commonConstBuffer;

static vkn::Buffer s_commonMeshDataBuffer;
static vkn::Buffer s_commonMaterialDataBuffer;
static vkn::Buffer s_commonTransformDataBuffer;
static vkn::Buffer s_commonInstDataBuffer;

static vkn::Buffer s_commonDrawIndirectCommandsBuffer;
static vkn::Buffer s_commonDrawIndirectCommandsCountBuffer;

static vkn::QueryPool s_commonQueryPool;

static std::vector<vkn::Texture>     s_commonMaterialTextures;
static std::vector<vkn::TextureView> s_commonMaterialTextureViews;
static std::vector<vkn::Sampler>     s_commonSamplers;

static std::array<vkn::Texture, (size_t)COMMON_DBG_TEX_IDX::COUNT>     s_commonDbgTextures;
static std::array<vkn::TextureView, (size_t)COMMON_DBG_TEX_IDX::COUNT> s_commonDbgTextureViews;

static std::vector<Vertex> s_cpuVertexBuffer;
static std::vector<IndexType> s_cpuIndexBuffer;

static std::vector<TextureLoadData> s_cpuTexturesData;

static std::vector<COMMON_MESH_INFO> s_cpuMeshData;
static std::vector<COMMON_MATERIAL>  s_cpuMaterialData;
static std::vector<glm::float4x4> s_cpuTransformData;
static std::vector<COMMON_INST_INFO> s_cpuInstData;

static GBuffer s_GBuffer;

static eng::Camera s_camera;
static glm::float3 s_cameraVel = M3D_ZEROF3;

static uint32_t s_dbgOutputRTIdx = 0;

static size_t s_frameNumber = 0;
static float s_frameTime = 0.f;
static bool s_swapchainRecreateRequired = false;
static bool s_flyCameraMode = false;

#ifdef ENG_BUILD_DEBUG
static bool s_useMeshIndirectDraw = true;
static bool s_useMeshCulling = true;
static bool s_useDepthPass = true;

// Uses for debug purposes during CPU frustum culling
static size_t s_dbgDrawnMeshCount = 0;
#else
static constexpr bool s_useMeshIndirectDraw = true;
static constexpr bool s_useMeshCulling = true;
static constexpr bool s_useDepthPass = true;
#endif


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

		ImGui::GetStyle().Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
		ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
		ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
		ImGui::GetStyle().Colors[ImGuiCol_Header] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
		ImGui::GetStyle().Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

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

        ImGui_ImplVulkan_InitInfo imGuiInitInfo = {};
        imGuiInitInfo.ApiVersion = s_vkInstance.GetApiVersion();
        imGuiInitInfo.Instance = s_vkInstance.Get();
        imGuiInitInfo.PhysicalDevice = s_vkPhysDevice.Get();
        imGuiInitInfo.Device = s_vkDevice.Get();
        imGuiInitInfo.QueueFamily = s_vkDevice.GetQueueFamilyIndex();
        imGuiInitInfo.Queue = s_vkDevice.GetQueue();
        imGuiInitInfo.DescriptorPoolSize = 1000;
        imGuiInitInfo.MinImageCount = 2;
        imGuiInitInfo.ImageCount = s_vkSwapchain.GetImageCount();
        imGuiInitInfo.PipelineCache = VK_NULL_HANDLE;

        imGuiInitInfo.UseDynamicRendering = true;
        imGuiInitInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT; // 0 defaults to VK_SAMPLE_COUNT_1_BIT
    #ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
        imGuiInitInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        imGuiInitInfo.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat = s_GBuffer.depthRT.GetFormat();
        
        imGuiInitInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        const VkFormat fmt = s_GBuffer.colorRTs[0].GetFormat();
        imGuiInitInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &fmt;
    #else
        #error Vulkan Dynamic Rendering Is Not Supported. Get Vulkan SDK Latests.
    #endif
        imGuiInitInfo.CheckVkResultFn = [](VkResult error) { VK_CHECK(error); };
        imGuiInitInfo.MinAllocationSize = 1024 * 1024;

        if (!ImGui_ImplVulkan_Init(&imGuiInitInfo)) {
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

            ImGui::SeparatorText("Common Info");
            ImGui::Text("Build Type: %s", BUILD_TYPE_STR);
            ImGui::Text("CPU: %.3f ms (%.1f FPS)", s_frameTime, 1000.f / s_frameTime);

            ImGui::NewLine();
            ImGui::SeparatorText("Memory Info");
            ImGui::Text("Vertex Buffer Size: %.3f MB", s_cpuVertexBuffer.size() * sizeof(Vertex) / 1024.f / 1024.f);
            ImGui::Text("Index Buffer Size: %.3f MB", s_cpuIndexBuffer.size() * sizeof(IndexType) / 1024.f / 1024.f);

            ImGui::NewLine();
            ImGui::SeparatorText("Camera Info");
            ImGui::Text("Fly Camera Mode (F5):");
            ImGui::SameLine(); 
            ImGui::TextColored(ImVec4(!s_flyCameraMode, s_flyCameraMode, 0.f, 1.f), s_flyCameraMode ? "ON" : "OFF");
            
            ImGui::NewLine();
            ImGui::SeparatorText("Debug Output");
            if (ImGui::BeginCombo("Render Target", DBG_RT_OUTPUT_NAMES[s_dbgOutputRTIdx])) {
                for (size_t i = 0; i < _countof(DBG_RT_OUTPUT_NAMES); ++i) {
                    const bool isSelected = (DBG_RT_OUTPUT_NAMES[i] == DBG_RT_OUTPUT_NAMES[s_dbgOutputRTIdx]);
                    
                    if (ImGui::Selectable(DBG_RT_OUTPUT_NAMES[i], isSelected)) {
                        s_dbgOutputRTIdx = i;
                    }
                    
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

        #ifdef ENG_BUILD_DEBUG
            static constexpr ImVec4 IMGUI_RED_COLOR(1.f, 0.f, 0.f, 1.f);
            static constexpr ImVec4 IMGUI_GREEN_COLOR(0.f, 1.f, 0.f, 1.f);

            ImGui::NewLine();
            ImGui::SeparatorText("Mesh Culling");
             ImGui::Checkbox("##MeshCullingEnabled", &s_useMeshCulling);
            ImGui::SameLine(); ImGui::TextColored(s_useMeshCulling ? IMGUI_GREEN_COLOR : IMGUI_RED_COLOR, "Enabled");

            ImGui::NewLine();
            ImGui::SeparatorText("Depth Pass");
            ImGui::Checkbox("##DepthPassEnabled", &s_useDepthPass);
            ImGui::SameLine(); ImGui::TextColored(s_useDepthPass ? IMGUI_GREEN_COLOR : IMGUI_RED_COLOR, "Enabled");

            ImGui::NewLine();
            ImGui::SeparatorText("GBuffer Pass");
            ImGui::Checkbox("##UseMeshIndirectDraw", &s_useMeshIndirectDraw);
            ImGui::SameLine(); ImGui::TextColored(s_useMeshIndirectDraw ? IMGUI_GREEN_COLOR : IMGUI_RED_COLOR, "Use Indirect Draw");
            
            if (!s_useMeshIndirectDraw) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.f, 1.f), "(Drawn Mesh Count: %zu)", s_dbgDrawnMeshCount);
            }
            ImGui::NewLine();
        #endif
        } ImGui::End();

        ImGui::Render();
    }


    static void Render(vkn::CmdBuffer& cmdBuffer)
    {
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

    VkDependencyInfo dependencyInfo = {};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageBarrier2;

    cmdBuffer.CmdPipelineBarrier2(dependencyInfo);
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

    VkDependencyInfo dependencyInfo = {};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.bufferMemoryBarrierCount = 1;
    dependencyInfo.pBufferMemoryBarriers = &bufferBarrier2;

    cmdBuffer.CmdPipelineBarrier2(dependencyInfo);
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
    
    VkCommandBufferSubmitInfo bufferSubmitInfo = {};
    bufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    bufferSubmitInfo.commandBuffer = vkCmdBuffer;
    bufferSubmitInfo.deviceMask = 0;

    VkSubmitInfo2 submitInfo2 = {};
    submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo2.waitSemaphoreInfoCount = vkWaitSemaphore != VK_NULL_HANDLE ? 1 : 0;
    submitInfo2.pWaitSemaphoreInfos = &waitSemaphoreInfo;
    submitInfo2.commandBufferInfoCount = 1;
    submitInfo2.pCommandBufferInfos = &bufferSubmitInfo;
    submitInfo2.signalSemaphoreInfoCount = vkSignalSemaphore != VK_NULL_HANDLE ? 1 : 0;
    submitInfo2.pSignalSemaphoreInfos = &signalSemaphoreInfo;

    VK_CHECK(vkQueueSubmit2(vkQueue, 1, &submitInfo2, vkFinishFence));
}


static void CreateVkInstance()
{
#ifdef ENG_VK_DEBUG_UTILS_ENABLED
    vkn::InstanceDebugMessengerCreateInfo dbgMessengerCreateInfo = {};
    dbgMessengerCreateInfo.messageType = 
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dbgMessengerCreateInfo.messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dbgMessengerCreateInfo.pMessageCallback = DbgVkMessageCallback;

    constexpr std::array instLayers = {
        "VK_LAYER_KHRONOS_validation",
    };
#endif

    constexpr std::array instExtensions = {
    #ifdef ENG_VK_DEBUG_UTILS_ENABLED
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    #endif

        VK_KHR_SURFACE_EXTENSION_NAME,
    #ifdef ENG_OS_WINDOWS
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    #endif
    };

    vkn::InstanceCreateInfo instCreateInfo = {};
    instCreateInfo.pApplicationName = APP_NAME;
    instCreateInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    instCreateInfo.pEngineName = "VkEngine";
    instCreateInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    instCreateInfo.apiVersion = VK_API_VERSION_1_3;
    instCreateInfo.extensions = instExtensions;
#ifdef ENG_VK_DEBUG_UTILS_ENABLED
    instCreateInfo.layers = instLayers;
    instCreateInfo.pDbgMessengerCreateInfo = &dbgMessengerCreateInfo;
#endif

    s_vkInstance.Create(instCreateInfo); 
    CORE_ASSERT(s_vkInstance.IsCreated());
}


static void CreateVkSwapchain()
{
    vkn::SwapchainCreateInfo swapchainCreateInfo = {};
    swapchainCreateInfo.pDevice = &s_vkDevice;
    swapchainCreateInfo.pSurface = &s_vkSurface;

    swapchainCreateInfo.width = s_pWnd->GetWidth();
    swapchainCreateInfo.height = s_pWnd->GetHeight();

    swapchainCreateInfo.minImageCount    = 2;
    swapchainCreateInfo.imageFormat      = VK_FORMAT_R8G8B8A8_SRGB;
    swapchainCreateInfo.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchainCreateInfo.imageArrayLayers = 1u;
    swapchainCreateInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCreateInfo.transform        = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainCreateInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode      = VSYNC_ENABLED ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

    s_vkSwapchain.Create(swapchainCreateInfo);
    CORE_ASSERT(s_vkSwapchain.IsCreated());
}


static void CreateVkPhysAndLogicalDevices()
{
    vkn::PhysicalDeviceFeaturesRequirenments physDeviceFeturesReq = {};
    physDeviceFeturesReq.independentBlend = true;
    physDeviceFeturesReq.descriptorBindingPartiallyBound = true;
    physDeviceFeturesReq.runtimeDescriptorArray = true;
    physDeviceFeturesReq.samplerAnisotropy = true;
    physDeviceFeturesReq.samplerMirrorClampToEdge = true;
    physDeviceFeturesReq.vertexPipelineStoresAndAtomics = true;

    vkn::PhysicalDevicePropertiesRequirenments physDevicePropsReq = {};
    physDevicePropsReq.deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

    vkn::PhysicalDeviceCreateInfo physDeviceCreateInfo = {};
    physDeviceCreateInfo.pInstance = &s_vkInstance;
    physDeviceCreateInfo.pPropertiesRequirenments = &physDevicePropsReq;
    physDeviceCreateInfo.pFeaturesRequirenments = &physDeviceFeturesReq;

    s_vkPhysDevice.Create(physDeviceCreateInfo);
    CORE_ASSERT(s_vkPhysDevice.IsCreated()); 

    constexpr std::array deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // Is used since ImGui hardcoded blend state count to 1 in it's pipeline, so validation layers complain
    // that ImGui pipeline has one blend state but VkRenderingInfo has more than one color attachments
    // TODO: Disable this feature/ Render ImGui in the end with separate pass with one color attachement
    VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT dynRendUnusedAttachmentsFeature = {};
    dynRendUnusedAttachmentsFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT;

    {
        VkPhysicalDeviceFeatures2 features2 = {};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &dynRendUnusedAttachmentsFeature;
    
        vkGetPhysicalDeviceFeatures2(s_vkPhysDevice.Get(), &features2);
        CORE_ASSERT_MSG(dynRendUnusedAttachmentsFeature.dynamicRenderingUnusedAttachments == VK_TRUE, "Unused attachmets physical device feature is not supported!");
    }


    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    features13.pNext = &dynRendUnusedAttachmentsFeature;

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

    vkn::DeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.pPhysDevice = &s_vkPhysDevice;
    deviceCreateInfo.pSurface = &s_vkSurface;
    deviceCreateInfo.queuePriority = 1.f;
    deviceCreateInfo.extensions = deviceExtensions;
    deviceCreateInfo.pFeatures2 = &features2;

    s_vkDevice.Create(deviceCreateInfo);
    CORE_ASSERT(s_vkDevice.IsCreated());
}


static void CreateCommonStagingBuffers()
{
    vkn::AllocationInfo stagingBufAllocInfo = {};
    stagingBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    stagingBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

    vkn::BufferCreateInfo stagingBufCreateInfo = {};
    stagingBufCreateInfo.pDevice = &s_vkDevice;
    stagingBufCreateInfo.size = STAGING_BUFFER_SIZE;
    stagingBufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufCreateInfo.pAllocInfo = &stagingBufAllocInfo;

    for (size_t i = 0; i < s_commonStagingBuffers.size(); ++i) {
        s_commonStagingBuffers[i].Create(stagingBufCreateInfo).SetDebugName("STAGING_BUFFER_%zu", i);
    }
}


static VkShaderModule CreateVkShaderModule(const fs::path& shaderSpirVPath, std::vector<uint8_t>* pExternalBuffer = nullptr)
{
    std::vector<uint8_t>* pShaderData = nullptr;
    std::vector<uint8_t> localBuffer;
    
    pShaderData = pExternalBuffer ? pExternalBuffer : &localBuffer;

    const std::string pathS = shaderSpirVPath.string();

    if (!ReadFile(*pShaderData, shaderSpirVPath)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", pathS.c_str());
    }
    VK_ASSERT_MSG(pShaderData->size() % sizeof(uint32_t) == 0, "Size of SPIR-V byte code of %s must be multiple of %zu", pathS.c_str(), sizeof(uint32_t));

    VkShaderModuleCreateInfo shaderCreateInfo = {};
    shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(pShaderData->data());
    shaderCreateInfo.codeSize = pShaderData->size();

    VkShaderModule shader = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(s_vkDevice.Get(), &shaderCreateInfo, nullptr, &shader));
    VK_ASSERT(shader != VK_NULL_HANDLE);

    return shader;
}


static void CreateCommonDescriptorPool()
{
    vkn::DescriptorPoolBuilder builder;

    builder
        // .SetFlags(VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT)
        .SetMaxDescriptorSetsCount(10);
        
    s_commonDescriptorSetPool = builder
        .AddResource(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100)
        .AddResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100)
        .AddResource(VK_DESCRIPTOR_TYPE_SAMPLER, (uint32_t)COMMON_SAMPLER_IDX::COUNT)
        .AddResource(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, COMMON_BINDLESS_TEXTURES_COUNT + (uint32_t)COMMON_DBG_TEX_IDX::COUNT)
        .Build();

    CORE_ASSERT(s_commonDescriptorSetPool != VK_NULL_HANDLE);
}


static void CreateCommonDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutBuilder builder;

    s_commonDescriptorSetLayout = builder
        // .SetFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT)
        .AddBinding(COMMON_SAMPLERS_DESCRIPTOR_SLOT,     VK_DESCRIPTOR_TYPE_SAMPLER, (uint32_t)COMMON_SAMPLER_IDX::COUNT, VK_SHADER_STAGE_ALL)
        .AddBinding(COMMON_CONST_BUFFER_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL)
        .AddBinding(COMMON_MESH_INFOS_DESCRIPTOR_SLOT,   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL)
        .AddBinding(COMMON_TRANSFORMS_DESCRIPTOR_SLOT,   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL)
        .AddBinding(COMMON_MATERIALS_DESCRIPTOR_SLOT,    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL)
        .AddBinding(COMMON_MTL_TEXTURES_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, COMMON_BINDLESS_TEXTURES_COUNT, VK_SHADER_STAGE_FRAGMENT_BIT)
        .AddBinding(COMMON_INST_INFOS_DESCRIPTOR_SLOT,   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL)
        .AddBinding(COMMON_VERTEX_DATA_DESCRIPTOR_SLOT,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
        .AddBinding(COMMON_DBG_TEXTURES_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, (uint32_t)COMMON_DBG_TEX_IDX::COUNT, VK_SHADER_STAGE_ALL)
        .Build();

    CORE_ASSERT(s_commonDescriptorSetLayout != VK_NULL_HANDLE);
}


static void CreateZPassDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutBuilder builder;

    s_zpassDescriptorSetLayout = builder
        // .SetFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT)
        .AddBinding(ZPASS_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .AddBinding(ZPASS_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build();

    CORE_ASSERT(s_zpassDescriptorSetLayout != VK_NULL_HANDLE);
}


static void CreateMeshCullingDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutBuilder builder;

    s_meshCullingDescriptorSetLayout = builder
        // .SetFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT)
        .AddBinding(MESH_CULLING_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(MESH_CULLING_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .Build();

    CORE_ASSERT(s_meshCullingDescriptorSetLayout != VK_NULL_HANDLE);
}


static void CreateGBufferDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutBuilder builder;

    s_gbufferRenderDescriptorSetLayout = builder
        // .SetFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT)
        .AddBinding(GBUFFER_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .AddBinding(GBUFFER_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build();

    CORE_ASSERT(s_gbufferRenderDescriptorSetLayout != VK_NULL_HANDLE);
}


static void AllocateDescriptorSets()
{
    vkn::DescriptorSetAllocator allocator;

    std::array descriptorSetsPairs = {
        std::make_pair(&s_commonDescriptorSetLayout,        &s_commonDescriptorSet),
        std::make_pair(&s_meshCullingDescriptorSetLayout,   &s_meshCullingDescriptorSet),
        std::make_pair(&s_zpassDescriptorSetLayout,         &s_zpassDescriptorSet),
        std::make_pair(&s_gbufferRenderDescriptorSetLayout, &s_gbufferRenderDescriptorSet),
    };

    std::array<VkDescriptorSet, descriptorSetsPairs.size()> descriptorSets;

    allocator.SetPool(s_commonDescriptorSetPool);

    for (auto& [pLayout, pSet] : descriptorSetsPairs) {
        allocator.AddLayout(*pLayout);
    }
    
    allocator.Allocate(descriptorSets);

    for (size_t i = 0; i < descriptorSets.size(); ++i) {
        auto& [pLayout, pSet] = descriptorSetsPairs[i];

        *pSet = descriptorSets[i];
        CORE_ASSERT(*pSet != VK_NULL_HANDLE);
    }
}


static void CreateDesriptorSets()
{
    CreateCommonDescriptorPool();

    CreateCommonDescriptorSetLayout();
    CreateZPassDescriptorSetLayout();
    CreateMeshCullingDescriptorSetLayout();
    CreateGBufferDescriptorSetLayout();

    AllocateDescriptorSets();
}


static void CreateMeshCullingPipelineLayout()
{
    vkn::PipelineLayoutBuilder builder(s_vkPhysDevice.GetProperties().limits.maxPushConstantsSize);

    s_meshCullingPipelineLayout = builder
        .AddPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MESH_CULLING_BINDLESS_REGISTRY))
        .AddDescriptorSetLayout(s_commonDescriptorSetLayout)
        .AddDescriptorSetLayout(s_meshCullingDescriptorSetLayout)
        .Build();

    CORE_ASSERT(s_meshCullingPipelineLayout != VK_NULL_HANDLE);
}


static void CreateZPassPipelineLayout()
{
    vkn::PipelineLayoutBuilder builder(s_vkPhysDevice.GetProperties().limits.maxPushConstantsSize);

    s_zpassPipelineLayout = builder
        .AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ZPASS_BINDLESS_REGISTRY))
        .AddDescriptorSetLayout(s_commonDescriptorSetLayout)
        .AddDescriptorSetLayout(s_zpassDescriptorSetLayout)
        .Build();

    CORE_ASSERT(s_zpassPipelineLayout != VK_NULL_HANDLE);
}


static void CreateGBufferPipelineLayout()
{
    vkn::PipelineLayoutBuilder builder(s_vkPhysDevice.GetProperties().limits.maxPushConstantsSize);

    s_gbufferRenderPipelineLayout = builder
        .AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GBUFFER_BINDLESS_REGISTRY))
        .AddDescriptorSetLayout(s_commonDescriptorSetLayout)
        .AddDescriptorSetLayout(s_gbufferRenderDescriptorSetLayout)
        .Build();

    CORE_ASSERT(s_gbufferRenderPipelineLayout != VK_NULL_HANDLE);
}


static void CreateMeshCullingPipeline(const fs::path& csPath)
{
    std::vector<uint8_t> shaderCodeBuffer;
    VkShaderModule shaderModule = CreateVkShaderModule(csPath, &shaderCodeBuffer);

    vkn::ComputePipelineBuilder builder;

    s_meshCullingPipeline = builder
        .SetShader(shaderModule, "main")
        .SetLayout(s_meshCullingPipelineLayout)
        .Build();
    
    vkDestroyShaderModule(s_vkDevice.Get(), shaderModule, nullptr);
    shaderModule = VK_NULL_HANDLE;

    CORE_ASSERT(s_meshCullingPipeline != VK_NULL_HANDLE);
}


static void CreateZPassPipeline(const fs::path& vsPath, const fs::path& psPath)
{
    std::vector<uint8_t> shaderCodeBuffer;
    std::array shaderModules = {
        CreateVkShaderModule(vsPath, &shaderCodeBuffer),
        CreateVkShaderModule(psPath, &shaderCodeBuffer),
    };

    const std::array shaderModuleStages = {
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    static_assert(shaderModules.size() == shaderModuleStages.size());
    const size_t shadersCount = shaderModules.size();

    vkn::GraphicsPipelineBuilder builder;

    for (size_t i = 0; i < shadersCount; ++i) {
        builder.AddShader(shaderModules[i], shaderModuleStages[i], "main");
    }
    
    builder
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
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .SetLayout(s_zpassPipelineLayout);

    builder.SetDepthAttachmentFormat(s_GBuffer.depthRT.GetFormat());
    
    s_zpassPipeline = builder.Build();

    for (VkShaderModule& shader : shaderModules) {
        vkDestroyShaderModule(s_vkDevice.Get(), shader, nullptr);
        shader = VK_NULL_HANDLE;
    }

    CORE_ASSERT(s_zpassPipeline != VK_NULL_HANDLE);
}
    

static void CreateGBufferRenderPipeline(const fs::path& vsPath, const fs::path& psPath)
{
    std::vector<uint8_t> shaderCodeBuffer;
    std::array shaderModules = {
        CreateVkShaderModule(vsPath, &shaderCodeBuffer),
        CreateVkShaderModule(psPath, &shaderCodeBuffer),
    };

    const std::array shaderModuleStages = {
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    static_assert(shaderModules.size() == shaderModuleStages.size());
    const size_t shadersCount = shaderModules.size();

    vkn::GraphicsPipelineBuilder builder;

    for (size_t i = 0; i < shadersCount; ++i) {
        builder.AddShader(shaderModules[i], shaderModuleStages[i], "main");
    }
    
    builder
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetStencilTestState(VK_FALSE, {}, {})
        .SetDepthTestState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_EQUAL)
        .SetDepthBoundsTestState(VK_TRUE, 0.f, 1.f)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .SetRasterizerLineWidth(1.f)
        .SetLayout(s_gbufferRenderPipelineLayout);

    #ifdef ENG_BUILD_DEBUG
        builder.AddDynamicState(std::array{ VK_DYNAMIC_STATE_DEPTH_COMPARE_OP, VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE });
    #else
        builder.SetDepthTestState(VK_TRUE, VK_FALSE, VK_COMPARE_OP_EQUAL);
    #endif

    VkPipelineColorBlendAttachmentState blendState = {};
    blendState.blendEnable = VK_FALSE;
    blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    for (const vkn::Texture& colorRT : s_GBuffer.colorRTs) {
        builder.AddColorAttachmentFormat(colorRT.GetFormat());  
        builder.AddColorBlendAttachment(blendState);   
    }
    builder.SetDepthAttachmentFormat(s_GBuffer.depthRT.GetFormat());
    
    s_gbufferRenderPipeline = builder.Build();

    for (VkShaderModule& shader : shaderModules) {
        vkDestroyShaderModule(s_vkDevice.Get(), shader, nullptr);
        shader = VK_NULL_HANDLE;
    }

    CORE_ASSERT(s_gbufferRenderPipeline != VK_NULL_HANDLE);
}




static void CreatePipelines()
{
    CreateMeshCullingPipelineLayout();
    CreateZPassPipelineLayout();
    CreateGBufferPipelineLayout();
    CreateMeshCullingPipeline("shaders/bin/mesh_culling.cs.spv");
    CreateZPassPipeline("shaders/bin/zpass.vs.spv", "shaders/bin/zpass.ps.spv");
    CreateGBufferRenderPipeline("shaders/bin/gbuffer.vs.spv", "shaders/bin/gbuffer.ps.spv");
}


static void CreateCommonDbgTextures()
{
#ifdef ENG_BUILD_DEBUG
    vkn::AllocationInfo allocInfo = {};
    allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    std::array<vkn::TextureCreateInfo, (size_t)COMMON_DBG_TEX_IDX::COUNT> texCreateInfos = {};

    for (vkn::TextureCreateInfo& createInfo : texCreateInfos) {
        createInfo.pDevice = &s_vkDevice;
        createInfo.type = VK_IMAGE_TYPE_2D;
        createInfo.extent = { 1U, 1U, 1U };
        createInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        createInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        createInfo.mipLevels = 1;
        createInfo.arrayLayers = 1;
        createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        createInfo.pAllocInfo = &allocInfo;
    }

    texCreateInfos[(size_t)COMMON_DBG_TEX_IDX::CHECKERBOARD].extent = { 16u, 16u, 1u };

    static constexpr std::array<const char*, (size_t)COMMON_DBG_TEX_IDX::COUNT> texNames = {
        "COMMON_DBG_TEX_RED",
        "COMMON_DBG_TEX_GREEN",
        "COMMON_DBG_TEX_BLUE",
        "COMMON_DBG_TEX_BLACK",
        "COMMON_DBG_TEX_WHITE",
        "COMMON_DBG_TEX_GREY",
        "COMMON_DBG_TEX_CHECKERBOARD",
    };

    for (size_t i = 0; i < s_commonDbgTextures.size(); ++i) {
        s_commonDbgTextures[i].Create(texCreateInfos[i]).SetDebugName(texNames[i]);
    }

    VkComponentMapping texMapping = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
            
    VkImageSubresourceRange texSubresourceRange = {};
    texSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    texSubresourceRange.baseMipLevel = 0;
    texSubresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    texSubresourceRange.baseArrayLayer = 0;
    texSubresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    for (size_t i = 0; i < s_commonDbgTextureViews.size(); ++i) {
        s_commonDbgTextureViews[i].Create(s_commonDbgTextures[i], texMapping, texSubresourceRange).SetDebugName(texNames[i]);
    }
#endif
}


static void UploadGPUDbgTextures()
{
#ifdef ENG_BUILD_DEBUG
    auto UploadDbgTexture = [](vkn::CmdBuffer& cmdBuffer, size_t texIdx, size_t stagingBufIdx) -> void
    {
        vkn::Texture& texture = s_commonDbgTextures[texIdx];

        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            texture.Get(),
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        VkCopyBufferToImageInfo2 copyInfo = {};

        copyInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
        copyInfo.srcBuffer = s_commonStagingBuffers[stagingBufIdx].Get();
        copyInfo.dstImage = texture.Get();
        copyInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copyInfo.regionCount = 1;

        VkBufferImageCopy2 texRegion = {};

        texRegion.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
        texRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        texRegion.imageSubresource.mipLevel = 0;
        texRegion.imageSubresource.baseArrayLayer = 0;
        texRegion.imageSubresource.layerCount = 1;
        texRegion.imageExtent = texture.GetSize();

        copyInfo.pRegions = &texRegion;

        vkCmdCopyBufferToImage2(cmdBuffer.Get(), &copyInfo);
    
        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_SHADER_READ_BIT,
            texture.Get(),
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    };

    vkn::Buffer& redImageStagingBuffer = s_commonStagingBuffers[0];

    uint8_t* pRedImageData = (uint8_t*)redImageStagingBuffer.Map(0, VK_WHOLE_SIZE);
    pRedImageData[0] = 255;
    pRedImageData[1] = 0;
    pRedImageData[2] = 0;
    pRedImageData[3] = 255;
    redImageStagingBuffer.Unmap();

    vkn::Buffer& greenImageStagingBuffer = s_commonStagingBuffers[1];

    uint8_t* pGreenImageData = (uint8_t*)greenImageStagingBuffer.Map(0, VK_WHOLE_SIZE);
    pGreenImageData[0] = 0;
    pGreenImageData[1] = 255;
    pGreenImageData[2] = 0;
    pGreenImageData[3] = 255;
    greenImageStagingBuffer.Unmap();

    size_t writeTexIdx = 0;

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        for (size_t i = 0; i < 2; ++i, ++writeTexIdx) {
            UploadDbgTexture(cmdBuffer, writeTexIdx, i);
        }
    });

    vkn::Buffer& blueImageStagingBuffer = s_commonStagingBuffers[0];

    uint8_t* pBlueImageData = (uint8_t*)blueImageStagingBuffer.Map(0, VK_WHOLE_SIZE);
    pBlueImageData[0] = 0;
    pBlueImageData[1] = 0;
    pBlueImageData[2] = 255;
    pBlueImageData[3] = 255;
    blueImageStagingBuffer.Unmap();

    vkn::Buffer& blackImageStagingBuffer = s_commonStagingBuffers[1];

    uint8_t* pBlackImageData = (uint8_t*)blackImageStagingBuffer.Map(0, VK_WHOLE_SIZE);
    pBlackImageData[0] = 0;
    pBlackImageData[1] = 0;
    pBlackImageData[2] = 0;
    pBlackImageData[3] = 255;
    blackImageStagingBuffer.Unmap();

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        for (size_t i = 0; i < 2; ++i, ++writeTexIdx) {
            UploadDbgTexture(cmdBuffer, writeTexIdx, i);
        }
    });

    vkn::Buffer& whiteImageStagingBuffer = s_commonStagingBuffers[0];

    uint8_t* pWhiteImageData = (uint8_t*)whiteImageStagingBuffer.Map(0, VK_WHOLE_SIZE);
    pWhiteImageData[0] = 255;
    pWhiteImageData[1] = 255;
    pWhiteImageData[2] = 255;
    pWhiteImageData[3] = 255;
    whiteImageStagingBuffer.Unmap();

    vkn::Buffer& greyImageStagingBuffer = s_commonStagingBuffers[1];

    uint8_t* pGreyImageData = (uint8_t*)greyImageStagingBuffer.Map(0, VK_WHOLE_SIZE);
    pGreyImageData[0] = 128;
    pGreyImageData[1] = 128;
    pGreyImageData[2] = 128;
    pGreyImageData[3] = 255;
    greyImageStagingBuffer.Unmap();

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        for (size_t i = 0; i < 2; ++i, ++writeTexIdx) {
            UploadDbgTexture(cmdBuffer, writeTexIdx, i);
        }
    });

    vkn::Buffer& checkerboardImageStagingBuffer = s_commonStagingBuffers[0];

    vkn::Texture& checkerboardTex = s_commonDbgTextures[(size_t)COMMON_DBG_TEX_IDX::CHECKERBOARD];

    uint32_t* pCheckerboardImageData = (uint32_t*)checkerboardImageStagingBuffer.Map(0, VK_WHOLE_SIZE);

    const uint32_t whiteColorU32 = glm::packUnorm4x8(glm::float4(1.f));
    const uint32_t blackColorU32 = glm::packUnorm4x8(glm::float4(0.f, 0.f, 0.f, 1.f));

    for (uint32_t y = 0; y < checkerboardTex.GetSizeY(); ++y) {
        for (uint32_t x = 0; x < checkerboardTex.GetSizeX(); ++x) {
            pCheckerboardImageData[y * checkerboardTex.GetSizeX() + x] = ((x % 2) ^ (y % 2)) ? whiteColorU32 : blackColorU32;
        }
    }
    checkerboardImageStagingBuffer.Unmap();

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        UploadDbgTexture(cmdBuffer, writeTexIdx, 0);
    });
#endif
}


static void CreateGBufferIndirectDrawBuffers()
{
    vkn::AllocationInfo allocInfo = {};
    allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo createInfo = {};
    createInfo.pDevice = &s_vkDevice;
    createInfo.size = MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(COMMON_INDIRECT_DRAW_CMD);
    createInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    createInfo.pAllocInfo = &allocInfo;

    s_commonDrawIndirectCommandsBuffer.Create(createInfo).SetDebugName("DRAW_INDIRECT_COMMAND_BUFFER");

    createInfo.size = sizeof(glm::uint);

    s_commonDrawIndirectCommandsCountBuffer.Create(createInfo).SetDebugName("DRAW_INDIRECT_COMMAND_COUNT_BUFFER");
}


static void CreateCommonSamplers()
{
    s_commonSamplers.resize((uint32_t)COMMON_SAMPLER_IDX::COUNT);

    std::vector<vkn::SamplerCreateInfo> samplerCreateInfo((uint32_t)COMMON_SAMPLER_IDX::COUNT);

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].pDevice = &s_vkDevice;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].magFilter = VK_FILTER_NEAREST;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].minFilter = VK_FILTER_NEAREST;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].mipLodBias = 0.f;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].anisotropyEnable = VK_FALSE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].compareEnable = VK_FALSE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].minLod = 0.f;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].maxLod = VK_LOD_CLAMP_NONE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT].unnormalizedCoordinates = VK_FALSE;

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT].addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT].addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT].addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE].addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE].addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE].addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER].addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER].addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER].addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER].borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE].addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE].addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE].addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT].magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT].minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT].mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT].addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT].addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT].addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE].addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE].addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE].addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER].addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER].addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER].addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER].borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE].addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE].addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE].addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;


    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_REPEAT].maxAnisotropy = 2.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRRORED_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRRORED_REPEAT].maxAnisotropy = 2.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_EDGE].maxAnisotropy = 2.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_BORDER] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_CLAMP_TO_BORDER].maxAnisotropy = 2.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRROR_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 2.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_REPEAT].maxAnisotropy = 2.f;

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRRORED_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRRORED_REPEAT].maxAnisotropy = 2.f;

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_EDGE].maxAnisotropy = 2.f;

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_BORDER] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_CLAMP_TO_BORDER].maxAnisotropy = 2.f;

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRROR_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_2X_LINEAR_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 2.f;


    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_REPEAT].maxAnisotropy = 4.f;

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRRORED_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRRORED_REPEAT].maxAnisotropy = 4.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_EDGE].maxAnisotropy = 4.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_BORDER] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_CLAMP_TO_BORDER].maxAnisotropy = 4.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRROR_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_NEAREST_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 4.f;

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_REPEAT].maxAnisotropy = 4.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRRORED_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRRORED_REPEAT].maxAnisotropy = 4.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_EDGE].maxAnisotropy = 4.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_BORDER] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_CLAMP_TO_BORDER].maxAnisotropy = 4.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRROR_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 4.f;


    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_REPEAT].maxAnisotropy = 8.f;

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRRORED_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRRORED_REPEAT].maxAnisotropy = 8.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_EDGE].maxAnisotropy = 8.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_BORDER] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_CLAMP_TO_BORDER].maxAnisotropy = 8.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRROR_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_NEAREST_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 8.f;

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_REPEAT].maxAnisotropy = 8.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRRORED_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRRORED_REPEAT].maxAnisotropy = 8.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_EDGE].maxAnisotropy = 8.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_BORDER] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_CLAMP_TO_BORDER].maxAnisotropy = 8.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRROR_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_8X_LINEAR_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 8.f;


    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_REPEAT].maxAnisotropy = 16.f;

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRRORED_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRRORED_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRRORED_REPEAT].maxAnisotropy = 16.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_EDGE].maxAnisotropy = 16.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_BORDER] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_BORDER];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_CLAMP_TO_BORDER].maxAnisotropy = 16.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRROR_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::NEAREST_MIRROR_CLAMP_TO_EDGE];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_NEAREST_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 16.f;

    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT].maxAnisotropy = 16.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRRORED_REPEAT] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRRORED_REPEAT];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRRORED_REPEAT].maxAnisotropy = 16.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_EDGE].maxAnisotropy = 16.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_BORDER] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_CLAMP_TO_BORDER].maxAnisotropy = 16.f;
    
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRROR_CLAMP_TO_EDGE] = samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::LINEAR_MIRROR_CLAMP_TO_EDGE];
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[(uint32_t)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 16.f;


    for (size_t i = 0; i < samplerCreateInfo.size(); ++i) {
        s_commonSamplers[i].Create(samplerCreateInfo[i]).SetDebugName(COMMON_SAMPLERS_DBG_NAMES[i]);
    }
}


static void WriteZPassCullingDescriptorSet()
{
    std::vector<VkWriteDescriptorSet> descWrites;

    VkDescriptorBufferInfo drawIndirectCommandsBufferInfo = {};
    drawIndirectCommandsBufferInfo.buffer = s_commonDrawIndirectCommandsBuffer.Get();
    drawIndirectCommandsBufferInfo.offset = 0;
    drawIndirectCommandsBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet drawIndirectCommandsBufferWrite = {};
    drawIndirectCommandsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawIndirectCommandsBufferWrite.dstSet = s_zpassDescriptorSet;
    drawIndirectCommandsBufferWrite.dstBinding = ZPASS_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT;
    drawIndirectCommandsBufferWrite.dstArrayElement = 0;
    drawIndirectCommandsBufferWrite.descriptorCount = 1;
    drawIndirectCommandsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    drawIndirectCommandsBufferWrite.pBufferInfo = &drawIndirectCommandsBufferInfo;

    descWrites.emplace_back(drawIndirectCommandsBufferWrite);

    VkDescriptorBufferInfo drawIndirectCommandsCountBufferInfo = {};
    drawIndirectCommandsCountBufferInfo.buffer = s_commonDrawIndirectCommandsCountBuffer.Get();
    drawIndirectCommandsCountBufferInfo.offset = 0;
    drawIndirectCommandsCountBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet drawIndirectCommandsCountBufferWrite = {};
    drawIndirectCommandsCountBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawIndirectCommandsCountBufferWrite.dstSet = s_zpassDescriptorSet;
    drawIndirectCommandsCountBufferWrite.dstBinding = ZPASS_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT;
    drawIndirectCommandsCountBufferWrite.dstArrayElement = 0;
    drawIndirectCommandsCountBufferWrite.descriptorCount = 1;
    drawIndirectCommandsCountBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    drawIndirectCommandsCountBufferWrite.pBufferInfo = &drawIndirectCommandsCountBufferInfo;

    descWrites.emplace_back(drawIndirectCommandsCountBufferWrite);

    vkUpdateDescriptorSets(s_vkDevice.Get(), descWrites.size(), descWrites.data(), 0, nullptr);
}


static void WriteMeshCullingDescriptorSet()
{
    std::vector<VkWriteDescriptorSet> descWrites;

    VkDescriptorBufferInfo drawIndirectCommandsBufferInfo = {};
    drawIndirectCommandsBufferInfo.buffer = s_commonDrawIndirectCommandsBuffer.Get();
    drawIndirectCommandsBufferInfo.offset = 0;
    drawIndirectCommandsBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet drawIndirectCommandsBufferWrite = {};
    drawIndirectCommandsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawIndirectCommandsBufferWrite.dstSet = s_meshCullingDescriptorSet;
    drawIndirectCommandsBufferWrite.dstBinding = MESH_CULLING_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT;
    drawIndirectCommandsBufferWrite.dstArrayElement = 0;
    drawIndirectCommandsBufferWrite.descriptorCount = 1;
    drawIndirectCommandsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    drawIndirectCommandsBufferWrite.pBufferInfo = &drawIndirectCommandsBufferInfo;

    descWrites.emplace_back(drawIndirectCommandsBufferWrite);

    VkDescriptorBufferInfo drawIndirectCommandsCountBufferInfo = {};
    drawIndirectCommandsCountBufferInfo.buffer = s_commonDrawIndirectCommandsCountBuffer.Get();
    drawIndirectCommandsCountBufferInfo.offset = 0;
    drawIndirectCommandsCountBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet drawIndirectCommandsCountBufferWrite = {};
    drawIndirectCommandsCountBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawIndirectCommandsCountBufferWrite.dstSet = s_meshCullingDescriptorSet;
    drawIndirectCommandsCountBufferWrite.dstBinding = MESH_CULLING_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT;
    drawIndirectCommandsCountBufferWrite.dstArrayElement = 0;
    drawIndirectCommandsCountBufferWrite.descriptorCount = 1;
    drawIndirectCommandsCountBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    drawIndirectCommandsCountBufferWrite.pBufferInfo = &drawIndirectCommandsCountBufferInfo;

    descWrites.emplace_back(drawIndirectCommandsCountBufferWrite);

    vkUpdateDescriptorSets(s_vkDevice.Get(), descWrites.size(), descWrites.data(), 0, nullptr);
}


static void WriteGBufferDescriptorSet()
{
    std::vector<VkWriteDescriptorSet> descWrites;

    VkDescriptorBufferInfo drawIndirectCommandsBufferInfo = {};
    drawIndirectCommandsBufferInfo.buffer = s_commonDrawIndirectCommandsBuffer.Get();
    drawIndirectCommandsBufferInfo.offset = 0;
    drawIndirectCommandsBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet drawIndirectCommandsBufferWrite = {};
    drawIndirectCommandsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawIndirectCommandsBufferWrite.dstSet = s_gbufferRenderDescriptorSet;
    drawIndirectCommandsBufferWrite.dstBinding = GBUFFER_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT;
    drawIndirectCommandsBufferWrite.dstArrayElement = 0;
    drawIndirectCommandsBufferWrite.descriptorCount = 1;
    drawIndirectCommandsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    drawIndirectCommandsBufferWrite.pBufferInfo = &drawIndirectCommandsBufferInfo;

    descWrites.emplace_back(drawIndirectCommandsBufferWrite);

    VkDescriptorBufferInfo drawIndirectCommandsCountBufferInfo = {};
    drawIndirectCommandsCountBufferInfo.buffer = s_commonDrawIndirectCommandsCountBuffer.Get();
    drawIndirectCommandsCountBufferInfo.offset = 0;
    drawIndirectCommandsCountBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet drawIndirectCommandsCountBufferWrite = {};
    drawIndirectCommandsCountBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawIndirectCommandsCountBufferWrite.dstSet = s_gbufferRenderDescriptorSet;
    drawIndirectCommandsCountBufferWrite.dstBinding = GBUFFER_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT;
    drawIndirectCommandsCountBufferWrite.dstArrayElement = 0;
    drawIndirectCommandsCountBufferWrite.descriptorCount = 1;
    drawIndirectCommandsCountBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    drawIndirectCommandsCountBufferWrite.pBufferInfo = &drawIndirectCommandsCountBufferInfo;

    descWrites.emplace_back(drawIndirectCommandsCountBufferWrite);

    vkUpdateDescriptorSets(s_vkDevice.Get(), descWrites.size(), descWrites.data(), 0, nullptr);
}


static void WriteCommonDescriptorSet()
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


    VkDescriptorBufferInfo commonMeshDataBufferInfo = {};
    commonMeshDataBufferInfo.buffer = s_commonMeshDataBuffer.Get();
    commonMeshDataBufferInfo.offset = 0;
    commonMeshDataBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet commonMeshDataBufferWrite = {};
    commonMeshDataBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    commonMeshDataBufferWrite.dstSet = s_commonDescriptorSet;
    commonMeshDataBufferWrite.dstBinding = COMMON_MESH_INFOS_DESCRIPTOR_SLOT;
    commonMeshDataBufferWrite.dstArrayElement = 0;
    commonMeshDataBufferWrite.descriptorCount = 1;
    commonMeshDataBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    commonMeshDataBufferWrite.pBufferInfo = &commonMeshDataBufferInfo;

    descWrites.emplace_back(commonMeshDataBufferWrite);


    VkDescriptorBufferInfo commonTransformDataBufferInfo = {};
    commonTransformDataBufferInfo.buffer = s_commonTransformDataBuffer.Get();
    commonTransformDataBufferInfo.offset = 0;
    commonTransformDataBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet commonTransformDataBufferWrite = {};
    commonTransformDataBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    commonTransformDataBufferWrite.dstSet = s_commonDescriptorSet;
    commonTransformDataBufferWrite.dstBinding = COMMON_TRANSFORMS_DESCRIPTOR_SLOT;
    commonTransformDataBufferWrite.dstArrayElement = 0;
    commonTransformDataBufferWrite.descriptorCount = 1;
    commonTransformDataBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    commonTransformDataBufferWrite.pBufferInfo = &commonTransformDataBufferInfo;

    descWrites.emplace_back(commonTransformDataBufferWrite);


    VkDescriptorBufferInfo commonMaterialDataBufferInfo = {};
    commonMaterialDataBufferInfo.buffer = s_commonMaterialDataBuffer.Get();
    commonMaterialDataBufferInfo.offset = 0;
    commonMaterialDataBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet commonMaterialDataBufferWrite = {};
    commonMaterialDataBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    commonMaterialDataBufferWrite.dstSet = s_commonDescriptorSet;
    commonMaterialDataBufferWrite.dstBinding = COMMON_MATERIALS_DESCRIPTOR_SLOT;
    commonMaterialDataBufferWrite.dstArrayElement = 0;
    commonMaterialDataBufferWrite.descriptorCount = 1;
    commonMaterialDataBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    commonMaterialDataBufferWrite.pBufferInfo = &commonMaterialDataBufferInfo;

    descWrites.emplace_back(commonMaterialDataBufferWrite);


    std::vector<VkDescriptorImageInfo> descImageInfos(s_commonMaterialTextureViews.size());
    descImageInfos.clear();

    for (size_t i = 0; i < s_commonMaterialTextureViews.size(); ++i) {
        VkDescriptorImageInfo descImageInfo = {};
        descImageInfo.imageView = s_commonMaterialTextureViews[i].Get();
        descImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        descImageInfos.emplace_back(descImageInfo);

        VkWriteDescriptorSet texWrite = {};
        texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        texWrite.dstSet = s_commonDescriptorSet;
        texWrite.dstBinding = COMMON_MTL_TEXTURES_DESCRIPTOR_SLOT;
        texWrite.dstArrayElement = i;
        texWrite.descriptorCount = 1;
        texWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        texWrite.pImageInfo = &descImageInfos.back();

        descWrites.emplace_back(texWrite);
    }


    VkDescriptorBufferInfo commonInstDataBufferInfo = {};
    commonInstDataBufferInfo.buffer = s_commonInstDataBuffer.Get();
    commonInstDataBufferInfo.offset = 0;
    commonInstDataBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet commonInstDataBufferWrite = {};
    commonInstDataBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    commonInstDataBufferWrite.dstSet = s_commonDescriptorSet;
    commonInstDataBufferWrite.dstBinding = COMMON_INST_INFOS_DESCRIPTOR_SLOT;
    commonInstDataBufferWrite.dstArrayElement = 0;
    commonInstDataBufferWrite.descriptorCount = 1;
    commonInstDataBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    commonInstDataBufferWrite.pBufferInfo = &commonInstDataBufferInfo;

    descWrites.emplace_back(commonInstDataBufferWrite);


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


#ifdef ENG_BUILD_DEBUG
    std::array<VkDescriptorImageInfo, (size_t)COMMON_DBG_TEX_IDX::COUNT> dbgDescImageInfos = {};

    for (size_t i = 0; i < s_commonDbgTextureViews.size(); ++i) {
        VkDescriptorImageInfo descImageInfo = {};
        descImageInfo.imageView = s_commonDbgTextureViews[i].Get();
        descImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        dbgDescImageInfos[i] = descImageInfo;

        VkWriteDescriptorSet texWrite = {};
        texWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        texWrite.dstSet = s_commonDescriptorSet;
        texWrite.dstBinding = COMMON_DBG_TEXTURES_DESCRIPTOR_SLOT;
        texWrite.dstArrayElement = i;
        texWrite.descriptorCount = 1;
        texWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        texWrite.pImageInfo = &dbgDescImageInfos[i];

        descWrites.emplace_back(texWrite);
    }
#endif
    
    vkUpdateDescriptorSets(s_vkDevice.Get(), descWrites.size(), descWrites.data(), 0, nullptr);
}


static void WriteDescriptorSets()
{
    WriteCommonDescriptorSet();
    WriteZPassCullingDescriptorSet();
    WriteMeshCullingDescriptorSet();
    WriteGBufferDescriptorSet();
}


static void LoadSceneMeshData(const gltf::Asset& asset)
{
    ENG_PROFILE_SCOPED_MARKER_C("Load_Scene_Mesh_Data", 255, 50, 255, 255);

    Timer timer;

    size_t vertexCount = 0;
    size_t indexCount = 0;
    size_t meshesCount = 0;

    auto GetVertexAttribAccessor = [](const gltf::Asset& asset, const gltf::Primitive& primitive, std::string_view name) -> const gltf::Accessor*
    {
        const fastgltf::Attribute* pAttrib = primitive.findAttribute(name); 
        return pAttrib != primitive.attributes.cend() ? &asset.accessors[pAttrib->accessorIndex] : nullptr;
    };

    for (const gltf::Mesh& mesh : asset.meshes) {
        const size_t submeshCount = mesh.primitives.size();

        meshesCount += submeshCount;

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

    s_cpuMeshData.reserve(meshesCount);
    s_cpuMeshData.clear();

    size_t sceneIdx = 0;

    for (const gltf::Mesh& mesh : asset.meshes) {
        for (size_t primIdx = 0; primIdx < mesh.primitives.size(); ++primIdx) {
            const gltf::Primitive& primitive = mesh.primitives[primIdx];
            
            const gltf::Accessor* pPosAccessor = GetVertexAttribAccessor(asset, primitive, "POSITION");
            CORE_ASSERT_MSG(pPosAccessor != nullptr, "Failed to find POSITION vertex attribute accessor for %zu primitive of %s mesh", primIdx, mesh.name.c_str());

            const gltf::Accessor* pNormAccessor = GetVertexAttribAccessor(asset, primitive, "NORMAL");
            CORE_ASSERT_MSG(pNormAccessor != nullptr, "Failed to find NORMAL vertex attribute accessor for %zu primitive of %s mesh", primIdx, mesh.name.c_str());
            
            const gltf::Accessor* pUvAccessor = GetVertexAttribAccessor(asset, primitive, "TEXCOORD_0");
            CORE_ASSERT_MSG(pUvAccessor != nullptr, "Failed to find TEXCOORD_0 vertex attribute accessor for %zu primitive of %s mesh", primIdx, mesh.name.c_str());

            const gltf::Accessor* pTangAccessor = GetVertexAttribAccessor(asset, primitive, "TANGENT");
            
            CORE_ASSERT(pPosAccessor->count == pNormAccessor->count);
            CORE_ASSERT(pPosAccessor->count == pUvAccessor->count);

            if (pTangAccessor) {
                CORE_ASSERT(pPosAccessor->count == pTangAccessor->count);
            } else {
                CORE_LOG_WARN("Failed to find TANGENT vertex attribute accessor for %zu primitive of %s mesh. Using runtime computed tangents", primIdx, mesh.name.c_str());
            }

            COMMON_MESH_INFO cpuMesh = {};

            cpuMesh.FIRST_VERTEX = s_cpuVertexBuffer.size();
            cpuMesh.VERTEX_COUNT = pPosAccessor->count;

            for (size_t vertIdx = 0; vertIdx < pPosAccessor->count; ++vertIdx) {
                const glm::float3 lpos = gltf::getAccessorElement<glm::float3>(asset, *pPosAccessor, vertIdx);
                const glm::float3 lnorm = glm::normalize(gltf::getAccessorElement<glm::float3>(asset, *pNormAccessor, vertIdx));
                const glm::float2 uv = gltf::getAccessorElement<glm::float2>(asset, *pUvAccessor, vertIdx);
                
                glm::float4 tang; 
                if (pTangAccessor) {
                    tang = gltf::getAccessorElement<glm::float4>(asset, *pTangAccessor, vertIdx);
                    tang = glm::float4(glm::normalize(glm::float3(tang.x, tang.y, tang.z)), tang.a);
                } else {
                    const glm::float3 binorm = !math::IsEqual(lnorm, -M3D_AXIS_Z) ? -M3D_AXIS_Z : -M3D_AXIS_Y;
                    tang = glm::float4(glm::normalize(glm::cross(lnorm, binorm)), 1.f);
                }
                
                Vertex vertex = {};
                vertex.Pack(lpos, lnorm, uv, tang);

                s_cpuVertexBuffer.emplace_back(vertex);
            }

            CORE_ASSERT(pPosAccessor->min.has_value());
            const auto& aabbLCSMin = pPosAccessor->min.value();
            CORE_ASSERT(aabbLCSMin.size() == 3);

            CORE_ASSERT(pPosAccessor->max.has_value());
            const auto& aabbLCSMax = pPosAccessor->max.value();
            CORE_ASSERT(aabbLCSMax.size() == 3);

            glm::float3 minVert;
            if (aabbLCSMin.isType<std::int64_t>()) {
                minVert = glm::float3(aabbLCSMin.get<std::int64_t>(0), aabbLCSMin.get<std::int64_t>(1), aabbLCSMin.get<std::int64_t>(2));
            } else {
                minVert = glm::float3(aabbLCSMin.get<double>(0), aabbLCSMin.get<double>(1), aabbLCSMin.get<double>(2));
            }
            
            glm::float3 maxVert;
            if (aabbLCSMax.isType<std::int64_t>()) {
                maxVert = glm::float3(aabbLCSMax.get<std::int64_t>(0), aabbLCSMax.get<std::int64_t>(1), aabbLCSMax.get<std::int64_t>(2));
            } else {
                maxVert = glm::float3(aabbLCSMax.get<double>(0), aabbLCSMax.get<double>(1), aabbLCSMax.get<double>(2));
            }

            const glm::float3 sphereBoundPosition = (minVert + maxVert) * 0.5f;

            cpuMesh.SPHERE_BOUNDS_CENTER_LCS = sphereBoundPosition;
            cpuMesh.SPHERE_BOUNDS_RADIUS_LCS = glm::max(glm::distance(minVert, sphereBoundPosition), glm::distance(maxVert, sphereBoundPosition));

            CORE_ASSERT_MSG(primitive.indicesAccessor.has_value(), "%zu primitive of %s mesh doesn't contation index accessor", primIdx, mesh.name.c_str());
            const gltf::Accessor& indexAccessor = asset.accessors[primitive.indicesAccessor.value()];

            CORE_ASSERT_MSG(indexAccessor.type == fastgltf::AccessorType::Scalar, "%zu primitive of %s mesh has invalid index accessor type", primIdx, mesh.name.c_str());
            
            cpuMesh.FIRST_INDEX = s_cpuIndexBuffer.size();
            cpuMesh.INDEX_COUNT = indexAccessor.count;
            
            if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedShort) {
                gltf::iterateAccessor<uint16_t>(asset, indexAccessor, 
                    [&](uint16_t index) {
                        s_cpuIndexBuffer.emplace_back(cpuMesh.FIRST_VERTEX + index);
                    }
                );
            } else if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedInt) {
                gltf::iterateAccessor<uint32_t>(asset, indexAccessor, 
                    [&](uint32_t index) {
                        s_cpuIndexBuffer.emplace_back(cpuMesh.FIRST_VERTEX + index);
                    }
                );
            } else {
                CORE_ASSERT_FAIL("Invalid index accessor component type: %u", static_cast<uint32_t>(indexAccessor.componentType));
            }

            s_cpuMeshData.emplace_back(cpuMesh);
        }
    }

    CORE_LOG_INFO("FastGLTF: Mesh loading finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void LoadSceneTexturesData(const gltf::Asset& asset, const fs::path& dirPath)
{
    ENG_PROFILE_SCOPED_MARKER_C("Load_Scene_Textures_Data", 255, 50, 255, 255);

    Timer timer;

    s_cpuTexturesData.reserve(asset.images.size());
    s_cpuTexturesData.clear();

    for (const gltf::Image& image : asset.images) {
        TextureLoadData texData = {};

        std::visit(
            gltf::visitor {
                [](const auto& arg){},
                [&](const gltf::sources::URI& filePath) {   
                    const fs::path path = filePath.uri.isLocalPath() ? fs::absolute(dirPath / filePath.uri.fspath()) : filePath.uri.fspath();
                    texData.Load(path);
                },
                [&](const gltf::sources::Vector& vector) {
                    texData.Load(vector.bytes.data(), vector.bytes.size());
                },
                [&](const gltf::sources::BufferView& view) {
                    const gltf::BufferView& bufferView = asset.bufferViews[view.bufferViewIndex];
                    const gltf::Buffer& buffer = asset.buffers[bufferView.bufferIndex];
    
                    std::visit(gltf::visitor {
                        [](const auto& arg){},
                        [&](const gltf::sources::Vector& vector) {
                            texData.Load(vector.bytes.data(), vector.bytes.size());                            
                        }
                    },
                    buffer.data);
                },
            },
        image.data);

        texData.SetName(image.name);

        s_cpuTexturesData.emplace_back(std::move(texData));
    }

    CORE_LOG_INFO("FastGLTF: Textures data loading finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void LoadSceneMaterialData(const gltf::Asset& asset)
{
    ENG_PROFILE_SCOPED_MARKER_C("Load_Scene_Material_Data", 255, 50, 255, 255);

    Timer timer;

    s_cpuMaterialData.reserve(asset.materials.size());
    s_cpuMaterialData.clear();

    for (const gltf::Material& material : asset.materials) {
        COMMON_MATERIAL mtl = { -1, -1, -1, -1, -1 };

        const auto& albedoTexOpt = material.pbrData.baseColorTexture;
        if (albedoTexOpt.has_value()) {
            const gltf::Texture& tex = asset.textures[albedoTexOpt.value().textureIndex];
            mtl.ALBEDO_TEX_IDX = tex.imageIndex.has_value() ? tex.imageIndex.value() : -1;
        }

        const auto& normalTexOpt = material.normalTexture;
        if (normalTexOpt.has_value()) {
            const gltf::Texture& tex = asset.textures[normalTexOpt.value().textureIndex];
            mtl.NORMAL_TEX_IDX = tex.imageIndex.has_value() ? tex.imageIndex.value() : -1;
        }

        const auto& mrTexOpt = material.pbrData.metallicRoughnessTexture;
        if (mrTexOpt.has_value()) {
            const gltf::Texture& tex = asset.textures[mrTexOpt.value().textureIndex];
            mtl.MR_TEX_IDX = tex.imageIndex.has_value() ? tex.imageIndex.value() : -1;
        }

        const auto& aoTexOpt = material.occlusionTexture;
        if (aoTexOpt.has_value()) {
            const gltf::Texture& tex = asset.textures[aoTexOpt.value().textureIndex];
            mtl.AO_TEX_IDX = tex.imageIndex.has_value() ? tex.imageIndex.value() : -1;
        }

        const auto& emissiveTexOpt = material.emissiveTexture;
        if (emissiveTexOpt.has_value()) {
            const gltf::Texture& tex = asset.textures[emissiveTexOpt.value().textureIndex];
            mtl.EMISSIVE_TEX_IDX = tex.imageIndex.has_value() ? tex.imageIndex.value() : -1;
        }

        s_cpuMaterialData.emplace_back(mtl);
    }

    CORE_LOG_INFO("FastGLTF: Materials data loading finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void LoadSceneInstData(const gltf::Asset& asset)
{
    ENG_PROFILE_SCOPED_MARKER_C("Load_Scene_Inst_Data", 255, 50, 255, 255);

    Timer timer;

    s_cpuInstData.reserve(asset.meshes.size());
    s_cpuInstData.clear();

    s_cpuTransformData.reserve(asset.nodes.size());
    s_cpuTransformData.clear();

    uint32_t meshIdx = 0;
    uint32_t trsIdx = 0;

    for (size_t sceneId = 0; sceneId < asset.scenes.size(); ++sceneId) {
        gltf::iterateSceneNodes(asset, sceneId, gltf::math::fmat4x4(1.f), [&](auto&& node, auto&& trs)
        {
            static_assert(sizeof(trs) == sizeof(glm::float4x4));
    
            glm::float4x4 transform(1.f);
            memcpy(&transform, &trs, sizeof(transform));
    
            s_cpuTransformData.emplace_back(transform);
    
            if (node.meshIndex.has_value()) {
                const gltf::Mesh& mesh = asset.meshes[node.meshIndex.value()];
    
                for (const gltf::Primitive& primitive : mesh.primitives) {
                    COMMON_INST_INFO instInfo = {};
                    
                    instInfo.MESH_IDX = meshIdx;
                    instInfo.TRANSFORM_IDX = trsIdx;
    
                    CORE_ASSERT(primitive.materialIndex.has_value());
                    instInfo.MATERIAL_IDX = primitive.materialIndex.value();
    
                    // TODO: support transparent objects rendering
                    if (primitive.materialIndex.has_value()) {
                        if (asset.materials[primitive.materialIndex.value()].alphaMode == gltf::AlphaMode::Opaque) {
                            s_cpuInstData.emplace_back(instInfo);
                        }
                    }
                    ++meshIdx;
                }
            }
    
            ++trsIdx;
        });
    }

    CORE_LOG_INFO("FastGLTF: Instance data loading finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void UploadGPUMeshData()
{
    ENG_PROFILE_SCOPED_MARKER_C("Upload_GPU_Mesh_Data", 255, 255, 0, 255);

    Timer timer;

    vkn::Buffer& stagingVertBuffer = s_commonStagingBuffers[0];

    const size_t gpuVertBufferSize = s_cpuVertexBuffer.size() * sizeof(Vertex);
    CORE_ASSERT(gpuVertBufferSize <= stagingVertBuffer.GetMemorySize());

    void* pVertexBufferData = stagingVertBuffer.Map(0, VK_WHOLE_SIZE);
    memcpy(pVertexBufferData, s_cpuVertexBuffer.data(), gpuVertBufferSize);
    stagingVertBuffer.Unmap();

    vkn::Buffer& stagingIndexBuffer = s_commonStagingBuffers[1];

    const size_t gpuIndexBufferSize = s_cpuIndexBuffer.size() * sizeof(IndexType);
    CORE_ASSERT(gpuIndexBufferSize <= stagingIndexBuffer.GetMemorySize());

    void* pIndexBufferData = stagingIndexBuffer.Map(0, VK_WHOLE_SIZE);
    memcpy(pIndexBufferData, s_cpuIndexBuffer.data(), gpuIndexBufferSize);
    stagingIndexBuffer.Unmap();

    vkn::AllocationInfo vertBufAllocInfo = {};
    vertBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    vertBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo vertBufCreateInfo = {};
    vertBufCreateInfo.pDevice = &s_vkDevice;
    vertBufCreateInfo.size = gpuVertBufferSize;
    vertBufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertBufCreateInfo.pAllocInfo = &vertBufAllocInfo;

    s_vertexBuffer.Create(vertBufCreateInfo).SetDebugName("COMMON_VB");

    vkn::AllocationInfo idxBufAllocInfo = {};
    idxBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    idxBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo idxBufCreateInfo = {};
    idxBufCreateInfo.pDevice = &s_vkDevice;
    idxBufCreateInfo.size = gpuIndexBufferSize;
    idxBufCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    idxBufCreateInfo.pAllocInfo = &idxBufAllocInfo;

    s_indexBuffer.Create(idxBufCreateInfo).SetDebugName("COMMON_IB");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        VkBufferCopy bufferRegion = {};
        
        bufferRegion.size = gpuVertBufferSize;
        vkCmdCopyBuffer(cmdBuffer.Get(), stagingVertBuffer.Get(), s_vertexBuffer.Get(), 1, &bufferRegion);

        bufferRegion.size = gpuIndexBufferSize;
        vkCmdCopyBuffer(cmdBuffer.Get(), stagingIndexBuffer.Get(), s_indexBuffer.Get(), 1, &bufferRegion);    
    });

    vkn::Buffer& stagingMeshInfosBuffer = s_commonStagingBuffers[0];

    const size_t meshDataBufferSize = s_cpuMeshData.size() * sizeof(COMMON_MESH_INFO);
    CORE_ASSERT(meshDataBufferSize <= stagingMeshInfosBuffer.GetMemorySize());

    void* pMeshBufferData = stagingMeshInfosBuffer.Map(0, VK_WHOLE_SIZE);
    memcpy(pMeshBufferData, s_cpuMeshData.data(), meshDataBufferSize);
    stagingMeshInfosBuffer.Unmap();

    vkn::AllocationInfo meshInfosBufAllocInfo = {};
    meshInfosBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    meshInfosBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo meshInfosBufCreateInfo = {};
    meshInfosBufCreateInfo.pDevice = &s_vkDevice;
    meshInfosBufCreateInfo.size = meshDataBufferSize;
    meshInfosBufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    meshInfosBufCreateInfo.pAllocInfo = &meshInfosBufAllocInfo;
    
    s_commonMeshDataBuffer.Create(meshInfosBufCreateInfo).SetDebugName("COMMON_MESH_DATA");

    vkn::Buffer& stagingTransformDataBuffer = s_commonStagingBuffers[1];

    const size_t trsDataBufferSize = s_cpuTransformData.size() * sizeof(s_cpuTransformData[0]);
    CORE_ASSERT(trsDataBufferSize <= stagingTransformDataBuffer.GetMemorySize());

    void* pData = stagingTransformDataBuffer.Map(0, VK_WHOLE_SIZE);
    memcpy(pData, s_cpuTransformData.data(), trsDataBufferSize);
    stagingTransformDataBuffer.Unmap();

    vkn::AllocationInfo commonTrsBufAllocInfo = {};
    commonTrsBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    commonTrsBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo commonTrsBufCreateInfo = {};
    commonTrsBufCreateInfo.pDevice = &s_vkDevice;
    commonTrsBufCreateInfo.size = trsDataBufferSize;
    commonTrsBufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    commonTrsBufCreateInfo.pAllocInfo = &commonTrsBufAllocInfo;

    s_commonTransformDataBuffer.Create(commonTrsBufCreateInfo).SetDebugName("COMMON_TRANSFORM_DATA");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        VkBufferCopy bufferRegion = {};
                
        bufferRegion.size = meshDataBufferSize;
        vkCmdCopyBuffer(cmdBuffer.Get(), stagingMeshInfosBuffer.Get(), s_commonMeshDataBuffer.Get(), 1, &bufferRegion);

        bufferRegion.size = trsDataBufferSize;
        vkCmdCopyBuffer(cmdBuffer.Get(), stagingTransformDataBuffer.Get(), s_commonTransformDataBuffer.Get(), 1, &bufferRegion);
    });

    CORE_LOG_INFO("FastGLTF: Mesh data GPU upload finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void UploadGPUTextureData()
{
    ENG_PROFILE_SCOPED_MARKER_C("Upload_GPU_Texture_Data", 255, 255, 0, 255);

    Timer timer;

    s_commonMaterialTextures.resize(s_cpuTexturesData.size());
    s_commonMaterialTextureViews.resize(s_cpuTexturesData.size());

    for (size_t i = 0; i < s_cpuTexturesData.size(); i += STAGING_BUFFER_COUNT) {
        for (size_t j = 0; j < STAGING_BUFFER_COUNT; ++j) {
            const size_t textureIdx = i + j;

            if (textureIdx >= s_cpuTexturesData.size()) {
                break;
            }

            const TextureLoadData& texData = s_cpuTexturesData[textureIdx];
            const size_t texSizeInBytes = texData.GetMemorySize();

            vkn::Buffer& stagingTexBuffer = s_commonStagingBuffers[j];
            CORE_ASSERT(texSizeInBytes <= stagingTexBuffer.GetMemorySize());

            void* pImageData = stagingTexBuffer.Map(0, VK_WHOLE_SIZE);
            memcpy(pImageData, texData.GetData(), texSizeInBytes);
            stagingTexBuffer.Unmap();

            vkn::AllocationInfo imageAllocInfo = {};
            imageAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
            imageAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

            vkn::TextureCreateInfo imageCreateInfo = {};

            imageCreateInfo.pDevice = &s_vkDevice;
            imageCreateInfo.type = VK_IMAGE_TYPE_2D;
            imageCreateInfo.extent.width = texData.GetWidth();
            imageCreateInfo.extent.height = texData.GetHeight();
            imageCreateInfo.extent.depth = 1;
            imageCreateInfo.format = texData.GetFormat();
            imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageCreateInfo.mipLevels = 1;
            imageCreateInfo.arrayLayers = 1;
            imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageCreateInfo.pAllocInfo = &imageAllocInfo;

            vkn::Texture& sceneImage = s_commonMaterialTextures[textureIdx];
            sceneImage.Create(imageCreateInfo).SetDebugName("COMMON_MTL_TEXTURE_%zu", textureIdx);

            VkComponentMapping mapping = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
            
            VkImageSubresourceRange subresourceRange = {};
            subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            subresourceRange.baseMipLevel = 0;
            subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
            subresourceRange.baseArrayLayer = 0;
            subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

            vkn::TextureView& sceneImageView = s_commonMaterialTextureViews[textureIdx];
            sceneImageView.Create(sceneImage, mapping, subresourceRange).SetDebugName("COMMON_MTL_TEXTURE_VIEW_%zu", textureIdx);
        }

        ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
            for (size_t j = 0; j < STAGING_BUFFER_COUNT; ++j) {
                const size_t textureIdx = i + j;

                if (textureIdx >= s_cpuTexturesData.size()) {
                    break;
                }

                vkn::Texture& image = s_commonMaterialTextures[textureIdx];

                CmdPipelineImageBarrier(
                    cmdBuffer,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_2_NONE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_NONE,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    image.Get(),
                    VK_IMAGE_ASPECT_COLOR_BIT
                );

                VkCopyBufferToImageInfo2 copyInfo = {};

                copyInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
                copyInfo.srcBuffer = s_commonStagingBuffers[j].Get();
                copyInfo.dstImage = image.Get();
                copyInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                copyInfo.regionCount = 1;

                VkBufferImageCopy2 texRegion = {};

                texRegion.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
                texRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                texRegion.imageSubresource.mipLevel = 0;
                texRegion.imageSubresource.baseArrayLayer = 0;
                texRegion.imageSubresource.layerCount = 1;
                texRegion.imageExtent = image.GetSize();

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
                    image.Get(),
                    VK_IMAGE_ASPECT_COLOR_BIT
                );
            }
        });
    }
}


static void UploadGPUMaterialData()
{
    ENG_PROFILE_SCOPED_MARKER_C("Upload_GPU_Material_Data", 255, 255, 0, 255);

    Timer timer;

    vkn::Buffer& stagingMtlDataBuffer = s_commonStagingBuffers[0];

    const size_t mtlDataBufferSize = s_cpuMaterialData.size() * sizeof(COMMON_MATERIAL);
    CORE_ASSERT(mtlDataBufferSize <= stagingMtlDataBuffer.GetMemorySize());

    void* pData = stagingMtlDataBuffer.Map(0, VK_WHOLE_SIZE);
    memcpy(pData, s_cpuMaterialData.data(), mtlDataBufferSize);
    stagingMtlDataBuffer.Unmap();

    vkn::AllocationInfo mtlBufAllocInfo = {};
    mtlBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    mtlBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo mtlBufCreateInfo = {};
    mtlBufCreateInfo.pDevice = &s_vkDevice;
    mtlBufCreateInfo.size = mtlDataBufferSize;
    mtlBufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    mtlBufCreateInfo.pAllocInfo = &mtlBufAllocInfo;

    s_commonMaterialDataBuffer.Create(mtlBufCreateInfo).SetDebugName("COMMON_MATERIAL_DATA");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        VkBufferCopy bufferRegion = {};
                
        bufferRegion.size = mtlDataBufferSize;
        vkCmdCopyBuffer(cmdBuffer.Get(), stagingMtlDataBuffer.Get(), s_commonMaterialDataBuffer.Get(), 1, &bufferRegion);
    });

    CORE_LOG_INFO("FastGLTF: Material data GPU upload finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void UploadGPUInstData()
{
    ENG_PROFILE_SCOPED_MARKER_C("Upload_GPU_Inst_Data", 255, 255, 0, 255);

    Timer timer;

    vkn::Buffer& stagingBuffer = s_commonStagingBuffers[0];

    const size_t bufferSize = s_cpuInstData.size() * sizeof(COMMON_INST_INFO);
    CORE_ASSERT(bufferSize <= stagingBuffer.GetMemorySize());

    void* pData = stagingBuffer.Map(0, VK_WHOLE_SIZE);
    memcpy(pData, s_cpuInstData.data(), bufferSize);
    stagingBuffer.Unmap();

    vkn::AllocationInfo instInfosBufAllocInfo = {};
    instInfosBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    instInfosBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo instInfosBufCreateInfo = {};
    instInfosBufCreateInfo.pDevice = &s_vkDevice;
    instInfosBufCreateInfo.size = bufferSize;
    instInfosBufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    instInfosBufCreateInfo.pAllocInfo = &instInfosBufAllocInfo;

    s_commonInstDataBuffer.Create(instInfosBufCreateInfo).SetDebugName("COMMON_INSTANCE_DATA");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        VkBufferCopy bufferRegion = {};
        bufferRegion.size = bufferSize;
        
        vkCmdCopyBuffer(cmdBuffer.Get(), stagingBuffer.Get(), s_commonInstDataBuffer.Get(), 1, &bufferRegion);
    });

    CORE_LOG_INFO("FastGLTF: Instance data GPU upload finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void UploadGPUResources()
{
    UploadGPUMeshData();
    UploadGPUInstData();
    UploadGPUTextureData();
    UploadGPUMaterialData();
    UploadGPUDbgTextures();
}


static void LoadScene(const fs::path& filepath)
{
    const std::string strPath = filepath.string();

    if (!fs::exists(filepath)) {
		CORE_ASSERT_FAIL("Unknown scene path: %s", strPath.c_str());
		return;
	}
    
    ENG_PROFILE_SCOPED_MARKER_C("Load_Scene", 255, 50, 255, 255);
    
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
    LoadSceneTexturesData(asset.get(), filepath.parent_path());
    LoadSceneMaterialData(asset.get());
    LoadSceneInstData(asset.get());

    CORE_LOG_INFO("\"%s\" loading finished: %f ms", strPath.c_str(), timer.End().GetDuration<float, std::milli>());
}


static void CreateGBuffer()
{
    vkn::AllocationInfo rtAllocInfo = {};
    rtAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    rtAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::TextureCreateInfo rtCreateInfo = {};
    rtCreateInfo.pDevice = &s_vkDevice;
    rtCreateInfo.type = VK_IMAGE_TYPE_2D;
    rtCreateInfo.extent = VkExtent3D{s_pWnd->GetWidth(), s_pWnd->GetHeight(), 1};
    rtCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    rtCreateInfo.flags = 0;
    rtCreateInfo.mipLevels = 1;
    rtCreateInfo.arrayLayers = 1;
    rtCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    rtCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    rtCreateInfo.pAllocInfo = &rtAllocInfo;

    VkComponentMapping mapping = {};
    mapping.r = VK_COMPONENT_SWIZZLE_R;
    mapping.g = VK_COMPONENT_SWIZZLE_G;
    mapping.b = VK_COMPONENT_SWIZZLE_B;
    mapping.a = VK_COMPONENT_SWIZZLE_A;
    
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;


    vkn::Texture& rt0 = s_GBuffer.colorRTs[GBuffer::RT_0];
    vkn::TextureView& rt0View = s_GBuffer.colorRTViews[GBuffer::RT_0];

    rtCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    rtCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    rt0.Create(rtCreateInfo).SetDebugName("COMMON_GBUFFER_0");
    rt0View.Create(rt0, mapping, subresourceRange).SetDebugName("COMMON_GBUFFER_0_VIEW");


    vkn::Texture& rt1 = s_GBuffer.colorRTs[GBuffer::RT_1];
    vkn::TextureView& rt1View = s_GBuffer.colorRTViews[GBuffer::RT_1];

    rtCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    rtCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    rt1.Create(rtCreateInfo).SetDebugName("COMMON_GBUFFER_1");
    rt1View.Create(rt1, mapping, subresourceRange).SetDebugName("COMMON_GBUFFER_1_VIEW");


    vkn::Texture& rt2 = s_GBuffer.colorRTs[GBuffer::RT_2];
    vkn::TextureView& rt2View = s_GBuffer.colorRTViews[GBuffer::RT_2];

    rtCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    rtCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    rt2.Create(rtCreateInfo).SetDebugName("COMMON_GBUFFER_2");
    rt2View.Create(rt2, mapping, subresourceRange).SetDebugName("COMMON_GBUFFER_2_VIEW");


    vkn::Texture& rt3 = s_GBuffer.colorRTs[GBuffer::RT_3];
    vkn::TextureView& rt3View = s_GBuffer.colorRTViews[GBuffer::RT_3];

    rtCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    rtCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    rt3.Create(rtCreateInfo).SetDebugName("COMMON_GBUFFER_3");
    rt3View.Create(rt3, mapping, subresourceRange).SetDebugName("COMMON_GBUFFER_3_VIEW");


    vkn::Texture& depthRT = s_GBuffer.depthRT;
    vkn::TextureView& depthRTView = s_GBuffer.depthRTView;

    rtCreateInfo.format = VK_FORMAT_D32_SFLOAT;
    rtCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    depthRT.Create(rtCreateInfo).SetDebugName("COMMON_DEPTH_RT");
    depthRTView.Create(depthRT, mapping, subresourceRange).SetDebugName("COMMON_DEPTH_RT_VIEW");
}


static void DestroyGBuffer()
{
    for (size_t i = 0; i < GBuffer::RT_COUNT; ++i) {
        s_GBuffer.colorRTViews[i].Destroy();
        s_GBuffer.colorRTs[i].Destroy();
    }

    s_GBuffer.depthRTView.Destroy();
    s_GBuffer.depthRT.Destroy();
}


static void ResizeGBuffer()
{
    DestroyGBuffer();
    CreateGBuffer();
}


static void CreateCommonConstBuffer()
{
    vkn::AllocationInfo allocInfo = {};
    allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    
    vkn::BufferCreateInfo createInfo = {};
    createInfo.pDevice = &s_vkDevice;
    createInfo.size = sizeof(COMMON_CB_DATA);
    createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    createInfo.pAllocInfo = &allocInfo;

    s_commonConstBuffer.Create(createInfo).SetDebugName("COMMON_CB");
}


void UpdateGPUCommonConstBuffer()
{
    ENG_PROFILE_SCOPED_MARKER_C("Update_Common_Const_Buffer", 255, 255, 50, 255);

    COMMON_CB_DATA* pCommonConstBufferData = s_commonConstBuffer.Map<COMMON_CB_DATA>();

    pCommonConstBufferData->COMMON_VIEW_MATRIX = s_camera.GetViewMatrix();
    pCommonConstBufferData->COMMON_PROJ_MATRIX = s_camera.GetProjMatrix();
    pCommonConstBufferData->COMMON_VIEW_PROJ_MATRIX = s_camera.GetViewProjMatrix();

    memcpy(&pCommonConstBufferData->COMMON_CAMERA_FRUSTUM, &s_camera.GetFrustum(), sizeof(FRUSTUM));
    
    uint32_t dbgVisFlags = DBG_RT_OUTPUT_MASKS[s_dbgOutputRTIdx];
    uint32_t dbgFlags = 0;

#ifdef ENG_BUILD_DEBUG
    dbgFlags |= s_useMeshIndirectDraw ? (uint32_t)COMMON_DBG_FLAG_MASKS::USE_MESH_INDIRECT_DRAW_MASK : 0;
    dbgFlags |= s_useMeshCulling ? (uint32_t)COMMON_DBG_FLAG_MASKS::USE_MESH_GPU_CULLING_MASK : 0;
#endif

    pCommonConstBufferData->COMMON_DBG_FLAGS = dbgFlags;
    pCommonConstBufferData->COMMON_DBG_VIS_FLAGS = dbgVisFlags;

    s_commonConstBuffer.Unmap();
}


void UpdateScene()
{
    DbgUI::BeginFrame();

    const float moveDist = glm::length(s_cameraVel);

    if (!math::IsZero(moveDist)) {
        const glm::float3 moveDir = glm::normalize(s_camera.GetRotation() * (s_cameraVel / moveDist));
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


void MeshCullingPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Mesh_Culling_Pass", 50, 50, 200, 255);

    vkCmdBindPipeline(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE, s_meshCullingPipeline);
    
    VkDescriptorSet descSets[] = { s_commonDescriptorSet, s_meshCullingDescriptorSet };
    vkCmdBindDescriptorSets(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE, s_meshCullingPipelineLayout, 0, _countof(descSets), descSets, 0, nullptr);

    MESH_CULLING_BINDLESS_REGISTRY registry = {};
    registry.INST_COUNT = s_cpuInstData.size();

    vkCmdPushConstants(cmdBuffer.Get(), s_meshCullingPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MESH_CULLING_BINDLESS_REGISTRY), &registry);

    vkCmdDispatch(cmdBuffer.Get(), (s_cpuInstData.size() + 63) / 64, 1, 1);

    CmdPipelineBufferBarrier(
        cmdBuffer, 
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT,
        s_commonDrawIndirectCommandsBuffer.Get()
    );

    CmdPipelineBufferBarrier(
        cmdBuffer, 
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT,
        s_commonDrawIndirectCommandsCountBuffer.Get()
    );
}


static bool IsInstVisible(const COMMON_INST_INFO& instInfo)
{
    ENG_PROFILE_SCOPED_MARKER_C("CPU_Is_Inst_Visible", 50, 200, 50, 255);

    const COMMON_MESH_INFO& mesh = s_cpuMeshData[instInfo.MESH_IDX];

    const glm::float4x4& wMatr = s_cpuTransformData[instInfo.TRANSFORM_IDX];

    const glm::float3 position = wMatr * glm::float4(mesh.SPHERE_BOUNDS_CENTER_LCS, 1.f);

    const float scale = glm::max(glm::max(glm::length(glm::float3(wMatr[0])), glm::length(glm::float3(wMatr[1]))), glm::length(glm::float3(wMatr[2])));
    const float radius = scale * mesh.SPHERE_BOUNDS_RADIUS_LCS;

    const math::Frustum& frustum = s_camera.GetFrustum();

    for (size_t i = 0; i < COMMON_FRUSTUM_PLANES_COUNT; ++i) {
        const math::Plane& plane = frustum.planes[i];

        if (glm::dot(plane.normal, position) + plane.distance < -radius) {
            return false;
        }
    }

    return true;
}


void DepthPass(vkn::CmdBuffer& cmdBuffer)
{
    if (!s_useDepthPass) {
        return;
    }

    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Depth_Pass", 128, 128, 128, 255);

    CmdPipelineImageBarrier(
        cmdBuffer,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_NONE,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_NONE,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        s_GBuffer.depthRT.Get(),
        VK_IMAGE_ASPECT_DEPTH_BIT
    );

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = s_GBuffer.depthRTView.Get();
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
        VkViewport viewport = {};
        viewport.width = renderingInfo.renderArea.extent.width;
        viewport.height = renderingInfo.renderArea.extent.height;
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        cmdBuffer.CmdSetViewport(0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.extent = renderingInfo.renderArea.extent;
        cmdBuffer.CmdSetScissor(0, 1, &scissor);

        vkCmdBindPipeline(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, s_zpassPipeline);
        
        VkDescriptorSet descSets[] = { s_commonDescriptorSet, s_zpassDescriptorSet };
        vkCmdBindDescriptorSets(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, s_zpassPipelineLayout, 0, _countof(descSets), descSets, 0, nullptr);

        cmdBuffer.CmdBindIndexBuffer(s_indexBuffer, 0, GetVkIndexType());

        if (s_useMeshIndirectDraw) {
            cmdBuffer.CmdDrawIndexedIndirect(s_commonDrawIndirectCommandsBuffer, 0, s_commonDrawIndirectCommandsCountBuffer, 0, MAX_INDIRECT_DRAW_CMD_COUNT, sizeof(COMMON_INDIRECT_DRAW_CMD));
        } else {
            ENG_PROFILE_SCOPED_MARKER_C("Depth_CPU_Frustum_Culling", 50, 255, 50, 255);

        #ifdef ENG_BUILD_DEBUG
            s_dbgDrawnMeshCount = 0;
        #endif

            for (uint32_t i = 0; i < s_cpuInstData.size(); ++i) {
                if (s_useMeshCulling) {
                    if (!IsInstVisible(s_cpuInstData[i])) {
                        continue;
                    }
                }

            #ifdef ENG_BUILD_DEBUG
                ++s_dbgDrawnMeshCount;
            #endif

                ZPASS_BINDLESS_REGISTRY registry = {};
                registry.INST_INFO_IDX = i;

                vkCmdPushConstants(cmdBuffer.Get(), s_zpassPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ZPASS_BINDLESS_REGISTRY), &registry);

                const COMMON_MESH_INFO& mesh = s_cpuMeshData[s_cpuInstData[i].MESH_IDX];
                cmdBuffer.CmdDrawIndexed(mesh.INDEX_COUNT, 1, mesh.FIRST_INDEX, mesh.FIRST_VERTEX, i);
            }
        }
    cmdBuffer.CmdEndRendering();

    CmdPipelineImageBarrier(
        cmdBuffer,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        s_GBuffer.depthRT.Get(),
        VK_IMAGE_ASPECT_DEPTH_BIT
    );
}


void GBufferRenderPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "GBuffer_Render_Pass", 50, 200, 50, 255);

    for (vkn::Texture& colorRT : s_GBuffer.colorRTs) {
        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            colorRT.Get(),
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    }

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = s_GBuffer.depthRTView.Get();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

#ifdef ENG_BUILD_DEBUG
    if (s_useDepthPass) {
        depthAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
    } else {
        depthAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;

        #ifdef ENG_REVERSED_Z
            depthAttachment.clearValue.depthStencil.depth = 0.f;
        #else
            depthAttachment.clearValue.depthStencil.depth = 1.f;
        #endif
    }
#else
    depthAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
#endif

    renderingInfo.pDepthAttachment = &depthAttachment;

    std::array<VkRenderingAttachmentInfo, GBuffer::RT_COUNT> colorAttachments = {};

    for (size_t i = 0; i < colorAttachments.size(); ++i) {
        VkRenderingAttachmentInfo& attachment = colorAttachments[i];

        attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        attachment.imageView = s_GBuffer.colorRTViews[i].Get();
        attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.clearValue.color.float32[0] = 0.f;
        attachment.clearValue.color.float32[1] = 0.f;
        attachment.clearValue.color.float32[2] = 0.f;
        attachment.clearValue.color.float32[3] = 0.f;
    }
    
    renderingInfo.colorAttachmentCount = colorAttachments.size();
    renderingInfo.pColorAttachments = colorAttachments.data();

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

        vkCmdBindPipeline(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, s_gbufferRenderPipeline);
        
        VkDescriptorSet descSets[] = { s_commonDescriptorSet, s_gbufferRenderDescriptorSet };
        vkCmdBindDescriptorSets(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, s_gbufferRenderPipelineLayout, 0, _countof(descSets), descSets, 0, nullptr);

        cmdBuffer.CmdBindIndexBuffer(s_indexBuffer, 0, GetVkIndexType());

    #ifdef ENG_BUILD_DEBUG
        if (s_useDepthPass) {
            cmdBuffer.CmdSetDepthCompareOp(VK_COMPARE_OP_EQUAL);
        } else {
            #ifdef ENG_REVERSED_Z
                cmdBuffer.CmdSetDepthCompareOp(VK_COMPARE_OP_GREATER_OR_EQUAL);
            #else
                cmdBuffer.CmdSetDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL);
            #endif
        }

        cmdBuffer.CmdSetDepthWriteEnable(s_useDepthPass ? VK_FALSE : VK_TRUE);
    #endif

        if (s_useMeshIndirectDraw) {
            cmdBuffer.CmdDrawIndexedIndirect(s_commonDrawIndirectCommandsBuffer, 0, s_commonDrawIndirectCommandsCountBuffer, 0, MAX_INDIRECT_DRAW_CMD_COUNT, sizeof(COMMON_INDIRECT_DRAW_CMD));
        } else {
            ENG_PROFILE_SCOPED_MARKER_C("GBuffer_CPU_Frustum_Culling", 50, 255, 50, 255);

        #ifdef ENG_BUILD_DEBUG
            s_dbgDrawnMeshCount = 0;
        #endif

            for (uint32_t i = 0; i < s_cpuInstData.size(); ++i) {
                if (s_useMeshCulling) {
                    if (!IsInstVisible(s_cpuInstData[i])) {
                        continue;
                    }
                }

            #ifdef ENG_BUILD_DEBUG
                ++s_dbgDrawnMeshCount;
            #endif

                GBUFFER_BINDLESS_REGISTRY registry = {};
                registry.INST_INFO_IDX = i;

                vkCmdPushConstants(cmdBuffer.Get(), s_gbufferRenderPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GBUFFER_BINDLESS_REGISTRY), &registry);

                const COMMON_MESH_INFO& mesh = s_cpuMeshData[s_cpuInstData[i].MESH_IDX];
                cmdBuffer.CmdDrawIndexed(mesh.INDEX_COUNT, 1, mesh.FIRST_INDEX, mesh.FIRST_VERTEX, i);
            }
        }
    cmdBuffer.CmdEndRendering();
}


static void DebugUIRenderPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Dbg_UI_Render_Pass", 200, 50, 50, 255);

    DbgUI::FillData();
    DbgUI::EndFrame();

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = s_GBuffer.depthRTView.Get();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;

    renderingInfo.pDepthAttachment = &depthAttachment;

    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = s_GBuffer.colorRTViews[GBuffer::RT_0].Get();
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    cmdBuffer.CmdBeginRendering(renderingInfo);
        DbgUI::Render(cmdBuffer);
    cmdBuffer.CmdEndRendering();
}


static void CopyRT0ToSwapchain(vkn::CmdBuffer& cmdBuffer, VkImage swapchainImage)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "RT0_Swapchain_Copy", 50, 200, 200, 255);

    CmdPipelineImageBarrier(
        cmdBuffer,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT,
        s_GBuffer.colorRTs[GBuffer::RT_0].Get(),
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    CmdPipelineImageBarrier(
        cmdBuffer,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_NONE,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_NONE,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        swapchainImage,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    VkCopyImageInfo2 rtToSwapchainTexCopyInfo = {};
    rtToSwapchainTexCopyInfo.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2;
    rtToSwapchainTexCopyInfo.srcImage = s_GBuffer.colorRTs[GBuffer::RT_0].Get();
    rtToSwapchainTexCopyInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    rtToSwapchainTexCopyInfo.dstImage = swapchainImage;
    rtToSwapchainTexCopyInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    rtToSwapchainTexCopyInfo.regionCount = 1;

    VkImageCopy2 cpyRegion = {};
    cpyRegion.sType = VK_STRUCTURE_TYPE_IMAGE_COPY_2;
    cpyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cpyRegion.srcSubresource.mipLevel = 0;
    cpyRegion.srcSubresource.baseArrayLayer = 0;
    cpyRegion.srcSubresource.layerCount = 1;
    cpyRegion.srcOffset = {};
    cpyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cpyRegion.dstSubresource.mipLevel = 0;
    cpyRegion.dstSubresource.baseArrayLayer = 0;
    cpyRegion.dstSubresource.layerCount = 1;
    cpyRegion.dstOffset = {};
    cpyRegion.extent = { s_vkSwapchain.GetImageExtent().width, s_vkSwapchain.GetImageExtent().height, 1U };
    
    rtToSwapchainTexCopyInfo.pRegions = &cpyRegion;

    vkCmdCopyImage2(cmdBuffer.Get(), &rtToSwapchainTexCopyInfo);

    CmdPipelineImageBarrier(
        cmdBuffer,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_ACCESS_2_NONE,
        swapchainImage,
        VK_IMAGE_ASPECT_COLOR_BIT
    );
}


static void RenderScene()
{
    if (s_renderFinishedFence.GetStatus() == VK_NOT_READY) {
        DbgUI::EndFrame();
        return;
    }

    ENG_PROFILE_SCOPED_MARKER_C("Render_Scene", 255, 255, 50, 255);

    UpdateGPUCommonConstBuffer();

    uint32_t nextImageIdx;
    const VkResult acquireResult = vkAcquireNextImageKHR(s_vkDevice.Get(), s_vkSwapchain.Get(), 10'000'000'000, s_presentFinishedSemaphore.Get(), VK_NULL_HANDLE, &nextImageIdx);
    
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

    cmdBuffer.Begin(cmdBeginInfo);
    {
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Render_Scene_GPU", 255, 165, 0, 255);

        MeshCullingPass(cmdBuffer);
        DepthPass(cmdBuffer);
        GBufferRenderPass(cmdBuffer);
        DebugUIRenderPass(cmdBuffer);
        
        CopyRT0ToSwapchain(cmdBuffer, s_vkSwapchain.GetImage(nextImageIdx));        

        ENG_PROFILE_GPU_COLLECT_STATS(cmdBuffer);
    }
    cmdBuffer.End();

    s_renderFinishedFence.Reset();

    SubmitVkQueue(
        s_vkDevice.GetQueue(),
        cmdBuffer.Get(),
        s_renderFinishedFence.Get(),
        s_presentFinishedSemaphore.Get(),
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

    const bool resizeResult = s_vkSwapchain.Resize(pWnd->GetWidth(), pWnd->GetHeight()).IsCreated();
    
    s_swapchainRecreateRequired = !resizeResult;

    return s_swapchainRecreateRequired;
}


static void CameraProcessWndEvent(eng::Camera& camera, const WndEvent& event)
{
    static bool firstEvent = true;

    if (event.Is<WndKeyEvent>()) {
        const WndKeyEvent& keyEvent = event.Get<WndKeyEvent>();

        if (keyEvent.IsPressed()) {
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

        static glm::float3 pitchYawRoll = s_camera.GetPitchYawRollDegrees();

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

            glm::float3 cameraDir;
            cameraDir.x = -glm::cos(glm::radians(pitchYawRoll.x)) * glm::sin(glm::radians(pitchYawRoll.y));
            cameraDir.y =  glm::sin(glm::radians(pitchYawRoll.x));
            cameraDir.z = -glm::cos(glm::radians(pitchYawRoll.x)) * glm::cos(glm::radians(pitchYawRoll.y));
            cameraDir = glm::normalize(cameraDir);

            const glm::float3 cameraRight = glm::normalize(glm::cross(cameraDir, M3D_AXIS_Y));
			const glm::float3 cameraUp    = glm::cross(cameraRight, cameraDir);
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

    if (event.Is<WndKeyEvent>()) {
        const WndKeyEvent& keyEvent = event.Get<WndKeyEvent>();

        if (keyEvent.key == WndKey::KEY_F5 && keyEvent.IsPressed()) {
            s_flyCameraMode = !s_flyCameraMode;
            s_pWnd->SetCursorRelativeMode(s_flyCameraMode);
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
        ResizeGBuffer();
    }

    UpdateScene();
    RenderScene();

    ++s_frameNumber;

    ENG_PROFILE_END_FRAME("Frame");
}


int main(int argc, char* argv[])
{
    wndSysInit();
    s_pWnd = wndSysGetMainWindow();

    WindowInitInfo wndInitInfo = {};
    wndInitInfo.pTitle = APP_NAME;
    wndInitInfo.width = 1280;
    wndInitInfo.height = 720;
    wndInitInfo.isVisible = false;

    s_pWnd->Create(wndInitInfo);
    ENG_ASSERT(s_pWnd->IsInitialized());

    CreateVkInstance();    

    vkn::SurfaceCreateInfo surfCreateInfo = {};
    surfCreateInfo.pInstance = &s_vkInstance;
    surfCreateInfo.pWndHandle = s_pWnd->GetNativeHandle();

    s_vkSurface.Create(surfCreateInfo);
    CORE_ASSERT(s_vkSurface.IsCreated());

    CreateVkPhysAndLogicalDevices();

#ifdef ENG_PROFILING_ENABLED
    vkn::GetProfiler().Create(&s_vkDevice);
    CORE_ASSERT(vkn::GetProfiler().IsCreated());
#endif

    vkn::AllocatorCreateInfo vkAllocatorCreateInfo = {}; 
    vkAllocatorCreateInfo.pDevice = &s_vkDevice;
    // RenderDoc doesn't work with buffer device address if you use VMA :(
    // vkAllocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    s_vkAllocator.Create(vkAllocatorCreateInfo);
    CORE_ASSERT(s_vkAllocator.IsCreated());

    CreateVkSwapchain();

    vkn::CmdPoolCreateInfo cmdPoolCreateInfo = {};
    cmdPoolCreateInfo.pDevice = &s_vkDevice;
    cmdPoolCreateInfo.queueFamilyIndex = s_vkDevice.GetQueueFamilyIndex();
    cmdPoolCreateInfo.flags =  VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    s_commonCmdPool.Create(cmdPoolCreateInfo).SetDebugName("COMMON_CMD_POOL");
    
    s_immediateSubmitCmdBuffer = s_commonCmdPool.AllocCmdBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    s_immediateSubmitCmdBuffer.SetDebugName("IMMEDIATE_CMD_BUFFER");

    s_immediateSubmitFinishedFence.Create(&s_vkDevice);

    CreateCommonStagingBuffers();

    CreateGBuffer();
    CreateCommonSamplers();
    CreateCommonConstBuffer();
    CreateGBufferIndirectDrawBuffers();
    CreateDesriptorSets();
    CreatePipelines();
    CreateCommonDbgTextures();

    DbgUI::Init();

    const size_t swapchainImageCount = s_vkSwapchain.GetImageCount();

    s_renderFinishedSemaphores.resize(swapchainImageCount);
    for (size_t i = 0; i < swapchainImageCount; ++i) {
        s_renderFinishedSemaphores[i].Create(&s_vkDevice).SetDebugName("RND_FINISH_SEMAPHORE_%zu", i);
    }
    s_presentFinishedSemaphore.Create(&s_vkDevice).SetDebugName("PRESENT_FINISH_SEMAPHORE");

    s_renderFinishedFence.Create(&s_vkDevice).SetDebugName("RND_FINISH_FENCE");
    
    s_renderCmdBuffer = s_commonCmdPool.AllocCmdBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    s_renderCmdBuffer.SetDebugName("RND_CMD_BUFFER");

    // LoadScene(argc > 1 ? argv[1] : "../assets/Sponza/Sponza.gltf");
    LoadScene(argc > 1 ? argv[1] : "../assets/LightSponza/Sponza.gltf");

    UploadGPUResources();
    WriteDescriptorSets();

    s_cpuTexturesData.clear();

    s_camera.SetPosition(glm::float3(0.f, 2.f, 0.f));
    s_camera.SetRotation(glm::quatLookAt(M3D_AXIS_X, M3D_AXIS_Y));
    s_camera.SetPerspProjection(glm::radians(90.f), (float)s_pWnd->GetWidth() / s_pWnd->GetHeight(), 0.01f, 10'000.f);

    s_pWnd->SetVisible(true);

    while(!s_pWnd->IsClosed()) {
        ProcessFrame();
    }

    s_vkDevice.WaitIdle();

    
    vkDestroyPipeline(s_vkDevice.Get(), s_zpassPipeline, nullptr);
    vkDestroyPipelineLayout(s_vkDevice.Get(), s_zpassPipelineLayout, nullptr);

    vkDestroyPipeline(s_vkDevice.Get(), s_meshCullingPipeline, nullptr);
    vkDestroyPipelineLayout(s_vkDevice.Get(), s_meshCullingPipelineLayout, nullptr);

    vkDestroyPipeline(s_vkDevice.Get(), s_gbufferRenderPipeline, nullptr);
    vkDestroyPipelineLayout(s_vkDevice.Get(), s_gbufferRenderPipelineLayout, nullptr);
    
    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_zpassDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_meshCullingDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_gbufferRenderDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_commonDescriptorSetLayout, nullptr);
    
    vkDestroyDescriptorPool(s_vkDevice.Get(), s_commonDescriptorSetPool, nullptr);

    DbgUI::Terminate();

    s_pWnd->Destroy();

    wndSysTerminate();

    return 0;
}