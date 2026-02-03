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
    enum class ComponentType : uint16_t
    {
        UINT8,
        UINT16,
        FLOAT,
        COUNT
    };

public:
    TextureLoadData() = default;
    
    TextureLoadData(const fs::path& filepath)
    {
        Load(filepath);
    }

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
        m_mipsCount = other.m_mipsCount;
        m_type = other.m_type;

        other.m_width = 0;
        other.m_height = 0;
        other.m_channels = 0;
        other.m_mipsCount = 1;
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
        m_mipsCount = CalcMipsCount(m_width, m_height);

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
        m_mipsCount = CalcMipsCount(m_width, m_height);

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
        m_mipsCount = 1;
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
    uint16_t GetMipsCount() const { return m_mipsCount; }

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

    static uint16_t CalcMipsCount(uint32_t width, uint32_t height)
    {
        return (uint16_t)glm::floor(glm::log2((float)glm::max(width, height))) + 1;
    }
    
private:
    static inline constexpr size_t COMP_TYPE_SIZE_IN_BYTES[] = { 1, 2, 4 };

    static_assert(_countof(COMP_TYPE_SIZE_IN_BYTES) == (size_t)ComponentType::COUNT);

private:
#ifdef ENG_VK_OBJ_DEBUG_NAME_ENABLED
    std::string m_name;
#endif

    void* m_pData = nullptr;
    VkFormat m_format = VK_FORMAT_UNDEFINED;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_channels = 0;

    uint16_t m_mipsCount = 1;
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


enum class COMMON_MATERIAL_FLAGS : glm::uint
{
    DOUBLE_SIDED = 0x1,
    ALPHA_KILL = 0x2,
    ALPHA_BLEND = 0x4,
};


struct COMMON_MATERIAL
{
    glm::float4 ALBEDO_MULT;

    glm::float3 EMISSIVE_MULT;
    float ALPHA_REF;

    float NORMAL_SCALE;
    float METALNESS_SCALE;
    float ROUGHNESS_SCALE;
    float AO_COEF;

    int32_t ALBEDO_TEX_IDX = -1;
    int32_t NORMAL_TEX_IDX = -1;
    int32_t MR_TEX_IDX = -1;
    int32_t AO_TEX_IDX = -1;

    glm::uvec2 PAD0;
    int32_t EMISSIVE_TEX_IDX = -1;
    glm::uint FLAGS;
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
    glm::float4x4 VIEW_MATRIX;
    glm::float4x4 PROJ_MATRIX;
    glm::float4x4 VIEW_PROJ_MATRIX;

    glm::float4x4 INV_VIEW_MATRIX;
    glm::float4x4 INV_PROJ_MATRIX;
    glm::float4x4 INV_VIEW_PROJ_MATRIX;

    FRUSTUM CAMERA_FRUSTUM;

    glm::uvec2 SCREEN_SIZE;
    float Z_NEAR;
    float Z_FAR;

    glm::uint  COMMON_FLAGS;
    glm::uint  COMMON_DBG_FLAGS;
    glm::uint  COMMON_DBG_VIS_FLAGS;
    glm::uint  PAD0;

    glm::float3 CAM_WPOS;
    glm::uint PAD1;
};


enum class COMMON_DBG_FLAG_MASKS
{
    USE_MESH_INDIRECT_DRAW_MASK = 0x1,
    USE_MESH_GPU_CULLING_MASK = 0x2,
    USE_REINHARD_TONE_MAPPING_MASK = 0x4,
    USE_PARTIAL_UNCHARTED_2_TONE_MAPPING_MASK = 0x8,
    USE_UNCHARTED_2_TONE_MAPPING_MASK = 0x10,
    USE_ACES_TONE_MAPPING_MASK = 0x20,
    USE_INDIRECT_LIGHTING_MASK = 0x40,
};


enum class COMMON_DBG_VIS_FLAG_MASKS
{
    DBG_VIS_NONE_MASK = 0x1,
    DBG_VIS_GBUFFER_ALBEDO_MASK = 0x2,
    DBG_VIS_GBUFFER_NORMAL_MASK = 0x4,
    DBG_VIS_GBUFFER_METALNESS_MASK = 0x8,
    DBG_VIS_GBUFFER_ROUGHNESS_MASK = 0x10,
    DBG_VIS_GBUFFER_AO_MASK = 0x20,
    DBG_VIS_GBUFFER_EMISSIVE_MASK = 0x40,
    DBG_VIS_VERT_NORMAL_MASK = 0x80,
    DBG_VIS_VERT_TANGENT_MASK = 0x100,
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


struct MESH_CULLING_PUSH_CONSTS
{
    glm::float3 PAD0;
    glm::uint INST_COUNT;
};


struct ZPASS_PUSH_CONSTS
{
    glm::uvec2 PAD0;
    glm::uint IS_AKILL_PASS;
    glm::uint INST_INFO_IDX;
};


struct GBUFFER_PUSH_CONSTS
{
    glm::uvec2 PAD0;
    glm::uint IS_AKILL_PASS;
    glm::uint INST_INFO_IDX;
};


struct IRRADIANCE_MAP_PUSH_CONSTS
{
    glm::uvec2 ENV_MAP_FACE_SIZE;
    glm::uvec2 PADDING;
};


struct PREFILTERED_ENV_MAP_PUSH_CONSTS
{
    glm::uvec2 ENV_MAP_FACE_SIZE;
    glm::uint  MIP;
    glm::uint  PADDING;
};


struct BRDF_INTEGRATION_PUSH_CONSTS
{
    glm::uvec4 PADDING;
};


static constexpr const char* DBG_RT_OUTPUT_NAMES[] = {
    "NONE",
    "GBUFFER ALBEDO",
    "GBUFFER NORMAL",
    "GBUFFER METALNESS",
    "GBUFFER ROUGHNESS",
    "GBUFFER AO",
    "GBUFFER EMISSIVE",
    "VERT NORMAL",
    "VERT TANGENT",
};


static constexpr uint32_t DBG_RT_OUTPUT_MASKS[] = {
    (uint32_t)COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_NONE_MASK,
    (uint32_t)COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_ALBEDO_MASK,
    (uint32_t)COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_NORMAL_MASK,
    (uint32_t)COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_METALNESS_MASK,
    (uint32_t)COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_ROUGHNESS_MASK,
    (uint32_t)COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_AO_MASK,
    (uint32_t)COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_EMISSIVE_MASK,
    (uint32_t)COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_VERT_NORMAL_MASK,
    (uint32_t)COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_VERT_TANGENT_MASK,
};


static_assert(_countof(DBG_RT_OUTPUT_NAMES) == _countof(DBG_RT_OUTPUT_MASKS));


static constexpr const char* DBG_TONEMAPPING_NAMES[] = {
    "REINHARD",
    "PARTIAL UNCHARTED 2",
    "UNCHARTED 2",
    "ACES",
};


static constexpr uint32_t TONEMAPPING_MASKS[] = {
    (uint32_t)COMMON_DBG_FLAG_MASKS::USE_REINHARD_TONE_MAPPING_MASK,
    (uint32_t)COMMON_DBG_FLAG_MASKS::USE_PARTIAL_UNCHARTED_2_TONE_MAPPING_MASK,
    (uint32_t)COMMON_DBG_FLAG_MASKS::USE_UNCHARTED_2_TONE_MAPPING_MASK,
    (uint32_t)COMMON_DBG_FLAG_MASKS::USE_ACES_TONE_MAPPING_MASK,
};


static_assert(_countof(DBG_TONEMAPPING_NAMES) == _countof(TONEMAPPING_MASKS));


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

static constexpr size_t MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT = 0;
static constexpr size_t MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT = 1;
static constexpr size_t MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT = 2;
static constexpr size_t MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT = 3;
static constexpr size_t MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT = 4;
static constexpr size_t MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT = 5;
static constexpr size_t MESH_CULL_OPAQUE_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT = 6;
static constexpr size_t MESH_CULL_AKILL_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT = 7;
static constexpr size_t MESH_CULL_TRANSP_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT = 8;

static constexpr size_t ZPASS_OPAQUE_INST_INFO_IDS_DESCRIPTOR_SLOT = 0;
static constexpr size_t ZPASS_AKILL_INST_INFO_IDS_DESCRIPTOR_SLOT = 1;

static constexpr size_t GBUFFER_OPAQUE_INST_INFO_IDS_DESCRIPTOR_SLOT = 0;
static constexpr size_t GBUFFER_AKILL_INST_INFO_IDS_DESCRIPTOR_SLOT = 1;

static constexpr size_t DEFERRED_LIGHTING_OUTPUT_UAV_DESCRIPTOR_SLOT = 0;
static constexpr size_t DEFERRED_LIGHTING_GBUFFER_0_DESCRIPTOR_SLOT = 1;
static constexpr size_t DEFERRED_LIGHTING_GBUFFER_1_DESCRIPTOR_SLOT = 2;
static constexpr size_t DEFERRED_LIGHTING_GBUFFER_2_DESCRIPTOR_SLOT = 3;
static constexpr size_t DEFERRED_LIGHTING_GBUFFER_3_DESCRIPTOR_SLOT = 4;
static constexpr size_t DEFERRED_LIGHTING_DEPTH_DESCRIPTOR_SLOT = 5;
static constexpr size_t DEFERRED_LIGHTING_IRRADIANCE_MAP_DESCRIPTOR_SLOT = 6;
static constexpr size_t DEFERRED_LIGHTING_PREFILTERED_ENV_MAP_DESCRIPTOR_SLOT = 7;
static constexpr size_t DEFERRED_LIGHTING_BRDF_LUT_DESCRIPTOR_SLOT = 8;

static constexpr size_t POST_PROCESSING_INPUT_COLOR_DESCRIPTOR_SLOT = 0;

static constexpr size_t SKYBOX_TEXTURE_DESCRIPTOR_SLOT = 0;

static constexpr size_t IRRADIANCE_MAP_GEN_ENV_MAP_DESCRIPTOR_SLOT = 0;
static constexpr size_t IRRADIANCE_MAP_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT = 1;

static constexpr size_t PREFILTERED_ENV_MAP_GEN_ENV_MAP_DESCRIPTOR_SLOT = 0;
static constexpr size_t PREFILTERED_ENV_MAP_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT = 1;

static constexpr size_t BRDF_INTEGRATION_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT = 0;


static constexpr uint32_t COMMON_BINDLESS_TEXTURES_COUNT = 128;

static constexpr uint32_t MAX_INDIRECT_DRAW_CMD_COUNT = 1024;

static constexpr size_t GBUFFER_RT_COUNT = 4;
static constexpr size_t CUBEMAP_FACE_COUNT = 6;

static constexpr size_t STAGING_BUFFER_SIZE  = 96 * 1024 * 1024; // 96 MB
static constexpr size_t STAGING_BUFFER_COUNT = 2;


static constexpr glm::uvec2 COMMON_IRRADIANCE_MAP_SIZE = glm::uvec2(32);
static constexpr glm::uvec2 COMMON_PREFILTERED_ENV_MAP_SIZE = glm::uvec2(256);
static constexpr glm::uvec2 COMMON_BRDF_INTEGRATION_LUT_SIZE = glm::uvec2(512);

static constexpr glm::uint  COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT = (glm::uint)math::CalcMipsCount(COMMON_PREFILTERED_ENV_MAP_SIZE.x);
static constexpr float      COMMON_PREFILTERED_ENV_MAP_MIP_ROUGHNESS_DELTA = 1.f / (COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1);

static constexpr const char* APP_NAME = "Vulkan Demo";

static constexpr bool VSYNC_ENABLED = false;

static constexpr float CAMERA_SPEED = 0.0025f;


static Window* s_pWnd = nullptr;

static vkn::Instance& s_vkInstance = vkn::GetInstance();
static vkn::Surface& s_vkSurface = vkn::GetSurface();

static vkn::PhysicalDevice& s_vkPhysDevice = vkn::GetPhysicalDevice();
static vkn::Device& s_vkDevice = vkn::GetDevice();

static vkn::Allocator& s_vkAllocator = vkn::GetAllocator();

static vkn::Swapchain& s_vkSwapchain = vkn::GetSwapchain();

static vkn::CmdPool s_commonCmdPool;

static vkn::CmdBuffer s_immediateSubmitCmdBuffer;
static vkn::Fence     s_immediateSubmitFinishedFence;

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

static VkDescriptorSet       s_deferredLightingDescriptorSet = VK_NULL_HANDLE;
static VkDescriptorSetLayout s_deferredLightingDescriptorSetLayout = VK_NULL_HANDLE;

static VkDescriptorSet       s_postProcessingDescriptorSet = VK_NULL_HANDLE;
static VkDescriptorSetLayout s_postProcessingDescriptorSetLayout = VK_NULL_HANDLE;

static VkDescriptorSet       s_skyboxDescriptorSet = VK_NULL_HANDLE;
static VkDescriptorSetLayout s_skyboxDescriptorSetLayout = VK_NULL_HANDLE;

static VkDescriptorSet       s_irradianceMapGenDescriptorSet = VK_NULL_HANDLE;
static VkDescriptorSetLayout s_irradianceMapGenDescriptorSetLayout = VK_NULL_HANDLE;

static std::array<VkDescriptorSet, COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT> s_prefilteredEnvGenDescriptorSets = {};
static VkDescriptorSetLayout s_prefilteredEnvMapGenDescriptorSetLayout = VK_NULL_HANDLE;

static VkDescriptorSet       s_BRDFIntegrationLUTGenDescriptorSet = VK_NULL_HANDLE;
static VkDescriptorSetLayout s_BRDFIntegrationLUTGenDescriptorSetLayout = VK_NULL_HANDLE;


static VkPipelineLayout s_meshCullingPipelineLayout = VK_NULL_HANDLE;
static VkPipeline       s_meshCullingPipeline = VK_NULL_HANDLE;

static VkPipelineLayout s_zpassPipelineLayout = VK_NULL_HANDLE;
static VkPipeline       s_zpassPipeline = VK_NULL_HANDLE;

static VkPipelineLayout s_gbufferRenderPipelineLayout = VK_NULL_HANDLE;
static VkPipeline       s_gbufferRenderPipeline = VK_NULL_HANDLE;

static VkPipelineLayout s_deferredLightingPipelineLayout = VK_NULL_HANDLE;
static VkPipeline       s_deferredLightingPipeline = VK_NULL_HANDLE;

static VkPipelineLayout s_postProcessingPipelineLayout = VK_NULL_HANDLE;
static VkPipeline       s_postProcessingPipeline = VK_NULL_HANDLE;

static VkPipelineLayout s_skyboxPipelineLayout = VK_NULL_HANDLE;
static VkPipeline       s_skyboxPipeline = VK_NULL_HANDLE;

static VkPipelineLayout s_irradianceMapGenPipelineLayout = VK_NULL_HANDLE;
static VkPipeline       s_irradianceMapGenPipeline = VK_NULL_HANDLE;

static VkPipelineLayout s_prefilteredEnvMapGenPipelineLayout = VK_NULL_HANDLE;
static VkPipeline       s_prefilteredEnvMapGenPipeline = VK_NULL_HANDLE;

static VkPipelineLayout s_BRDFIntegrationLUTGenPipelineLayout = VK_NULL_HANDLE;
static VkPipeline       s_BRDFIntegrationLUTGenPipeline = VK_NULL_HANDLE;


static vkn::Buffer s_vertexBuffer;
static vkn::Buffer s_indexBuffer;

static vkn::Buffer s_commonConstBuffer;

static vkn::Buffer s_commonMeshDataBuffer;
static vkn::Buffer s_commonMaterialDataBuffer;
static vkn::Buffer s_commonTransformDataBuffer;
static vkn::Buffer s_commonInstDataBuffer;

static vkn::Buffer s_commonOpaqueMeshDrawCmdBuffer;
static vkn::Buffer s_commonOpaqueMeshDrawCmdCountBuffer;
static vkn::Buffer s_commonCulledOpaqueInstInfoIDsBuffer;

static vkn::Buffer s_commonAKillMeshDrawCmdBuffer;
static vkn::Buffer s_commonAKillMeshDrawCmdCountBuffer;
static vkn::Buffer s_commonCulledAKillInstInfoIDsBuffer;

static vkn::Buffer s_commonTranspMeshDrawCmdBuffer;
static vkn::Buffer s_commonTranspMeshDrawCmdCountBuffer;
static vkn::Buffer s_commonCulledTranspInstInfoIDsBuffer;

static std::vector<vkn::Texture>     s_commonMaterialTextures;
static std::vector<vkn::TextureView> s_commonMaterialTextureViews;
static std::vector<vkn::Sampler>     s_commonSamplers;

static std::vector<Vertex> s_cpuVertexBuffer;
static std::vector<IndexType> s_cpuIndexBuffer;

static std::vector<TextureLoadData> s_cpuTexturesData;

static std::vector<COMMON_MESH_INFO> s_cpuMeshData;
static std::vector<COMMON_MATERIAL>  s_cpuMaterialData;
static std::vector<glm::float4x4>    s_cpuTransformData;
static std::vector<COMMON_INST_INFO> s_cpuInstData;


static std::array<vkn::Texture, (size_t)COMMON_DBG_TEX_IDX::COUNT>     s_commonDbgTextures;
static std::array<vkn::TextureView, (size_t)COMMON_DBG_TEX_IDX::COUNT> s_commonDbgTextureViews;

static vkn::Texture     s_skyboxTexture;
static vkn::TextureView s_skyboxTextureView;

static vkn::Texture     s_irradianceMapTexture;
static vkn::TextureView s_irradianceMapTextureView;
static vkn::TextureView s_irradianceMapTextureViewRW;

static vkn::Texture     s_prefilteredEnvMapTexture;
static vkn::TextureView s_prefilteredEnvMapTextureView;
static std::array<vkn::TextureView, COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT> s_prefilteredEnvMapTextureViewRWs;

static vkn::Texture     s_brdfLUTTexture;
static vkn::TextureView s_brdfLUTTextureView;
static vkn::TextureView s_brdfLUTTextureViewRW;

static std::array<vkn::Texture, GBUFFER_RT_COUNT>     s_gbufferRTs;
static std::array<vkn::TextureView, GBUFFER_RT_COUNT> s_gbufferRTViews;

static vkn::Texture     s_commonDepthRT;
static vkn::TextureView s_commonDepthRTView;

static vkn::Texture     s_colorRT;
static vkn::TextureView s_colorRTView;

static eng::Camera s_camera;
static glm::float3 s_cameraVel = M3D_ZEROF3;

static uint32_t s_dbgOutputRTIdx = 0;
static uint32_t s_nextImageIdx = 0;

static size_t s_frameNumber = 0;
static float s_frameTime = 0.f;
static bool s_swapchainRecreateRequired = false;
static bool s_flyCameraMode = false;

#ifdef ENG_BUILD_DEBUG
    static bool s_useMeshIndirectDraw = true;
    static bool s_useMeshCulling = true;
    static bool s_useDepthPass = true;
    static bool s_useIndirectLighting = true;

    // Uses for debug purposes during CPU frustum culling
    static size_t s_dbgDrawnOpaqueMeshCount = 0;
    static size_t s_dbgDrawnAkillMeshCount = 0;
    static size_t s_dbgDrawnTranspMeshCount = 0;

    static uint32_t s_tonemappingPreset = _countof(TONEMAPPING_MASKS) - 1;
#else
    static constexpr bool s_useMeshIndirectDraw = true;
    static constexpr bool s_useMeshCulling = true;
    static constexpr bool s_useDepthPass = true;
    static constexpr bool s_useIndirectLighting = true;

    static constexpr uint32_t s_tonemappingPreset = _countof(TONEMAPPING_MASKS) - 1;

    static_assert(s_tonemappingPreset < _countof(TONEMAPPING_MASKS));
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
        
        imGuiInitInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        const VkFormat fmt = s_vkSwapchain.GetImageFormat();
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

            VmaBudget budgets[VK_MAX_MEMORY_HEAPS] = {};
            vmaGetHeapBudgets(vkn::GetAllocator().Get(), budgets);

            for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; ++i) {
                const VmaBudget& budget = budgets[i];
                
                if (budget.usage > 0) {
                    const float usageMB = budget.usage / 1024.f / 1024.f;
                    const float budgetMB = budget.budget / 1024.f / 1024.f;
                    ImGui::Text("Heap %u: Usage: %.2f / %.2f MB (%.2f%%)", i, usageMB, budgetMB, usageMB / budgetMB * 100.f);
                }
            }

            ImGui::Text("Vertex Buffer Size: %.3f MB", s_cpuVertexBuffer.size() * sizeof(Vertex) / 1024.f / 1024.f);
            ImGui::Text("Index Buffer Size: %.3f MB", s_cpuIndexBuffer.size() * sizeof(IndexType) / 1024.f / 1024.f);

            ImGui::NewLine();
            ImGui::SeparatorText("Camera Info");
            ImGui::Text("Fly Camera Mode (F5):");
            ImGui::SameLine(); 
            ImGui::TextColored(ImVec4(!s_flyCameraMode, s_flyCameraMode, 0.f, 1.f), s_flyCameraMode ? "ON" : "OFF");

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
                ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.f, 1.f), "(Drawn Opaque Mesh Count: %zu)", s_dbgDrawnOpaqueMeshCount);
                ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.f, 1.f), "(Drawn AKill Mesh Count: %zu)", s_dbgDrawnAkillMeshCount);
                ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.f, 1.f), "(Drawn Transparent Mesh Count: %zu)", s_dbgDrawnTranspMeshCount);
            }

            ImGui::NewLine();
            ImGui::SeparatorText("Deferred Lighting Pass");
            ImGui::Checkbox("##UseIndirectLighting", &s_useIndirectLighting);
            ImGui::SameLine(); ImGui::TextColored(s_useIndirectLighting ? IMGUI_GREEN_COLOR : IMGUI_RED_COLOR, "Use Indirect Lighting");
            
            ImGui::NewLine();
            ImGui::SeparatorText("Tonemapping");
            if (ImGui::BeginCombo("Preset", DBG_TONEMAPPING_NAMES[s_tonemappingPreset])) {
                for (size_t i = 0; i < _countof(DBG_TONEMAPPING_NAMES); ++i) {
                    const bool isSelected = (DBG_TONEMAPPING_NAMES[i] == DBG_TONEMAPPING_NAMES[s_tonemappingPreset]);
                    
                    if (ImGui::Selectable(DBG_TONEMAPPING_NAMES[i], isSelected)) {
                        s_tonemappingPreset = i;
                    }
                    
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

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
        #endif
        } ImGui::End();

        ImGui::Render();
    }


    static void Render(vkn::CmdBuffer& cmdBuffer)
    {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer.Get());
    }
}


static bool IsAKillMaterial(const COMMON_MATERIAL& material)
{
    return (material.FLAGS & (uint32_t)COMMON_MATERIAL_FLAGS::ALPHA_KILL) != 0;
}


static bool IsTransparentMaterial(const COMMON_MATERIAL& material)
{
    return (material.FLAGS & (uint32_t)COMMON_MATERIAL_FLAGS::ALPHA_BLEND) != 0;
}


static bool IsOpaqueMaterial(const COMMON_MATERIAL& material)
{
    return !IsAKillMaterial(material) && !IsTransparentMaterial(material);
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
    VkImageAspectFlags aspectMask,
    uint32_t baseMipLevel = 0,
    uint32_t levelCount = VK_REMAINING_MIP_LEVELS,
    uint32_t baseArrayLayer = 0,
    uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS
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
    imageBarrier2.subresourceRange.baseMipLevel = baseMipLevel;
    imageBarrier2.subresourceRange.levelCount = levelCount;
    imageBarrier2.subresourceRange.baseArrayLayer = baseArrayLayer;
    imageBarrier2.subresourceRange.layerCount = layerCount;

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

    bool succeded;
    s_vkSwapchain.Create(swapchainCreateInfo, succeded);

    CORE_ASSERT(succeded && s_vkSwapchain.IsCreated());
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


static void CreateDynamicRenderTargets()
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

    VkComponentMapping mapping = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;


    vkn::Texture& gbuffRT0 = s_gbufferRTs[0];
    vkn::TextureView& gbuffRT0View = s_gbufferRTViews[0];

    rtCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    rtCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    gbuffRT0.Create(rtCreateInfo).SetDebugName("COMMON_GBUFFER_0");
    gbuffRT0View.Create(gbuffRT0, mapping, subresourceRange).SetDebugName("COMMON_GBUFFER_0_VIEW");


    vkn::Texture& gbuffRT1 = s_gbufferRTs[1];
    vkn::TextureView& gbuffRT1View = s_gbufferRTViews[1];

    rtCreateInfo.format = VK_FORMAT_R16G16B16A16_SNORM;
    rtCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    gbuffRT1.Create(rtCreateInfo).SetDebugName("COMMON_GBUFFER_1");
    gbuffRT1View.Create(gbuffRT1, mapping, subresourceRange).SetDebugName("COMMON_GBUFFER_1_VIEW");


    vkn::Texture& gbuffRT2 = s_gbufferRTs[2];
    vkn::TextureView& gbuffRT2View = s_gbufferRTViews[2];

    rtCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    rtCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    gbuffRT2.Create(rtCreateInfo).SetDebugName("COMMON_GBUFFER_2");
    gbuffRT2View.Create(gbuffRT2, mapping, subresourceRange).SetDebugName("COMMON_GBUFFER_2_VIEW");


    vkn::Texture& gbuffRT3 = s_gbufferRTs[3];
    vkn::TextureView& gbuffRT3View = s_gbufferRTViews[3];

    rtCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    rtCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    gbuffRT3.Create(rtCreateInfo).SetDebugName("COMMON_GBUFFER_3");
    gbuffRT3View.Create(gbuffRT3, mapping, subresourceRange).SetDebugName("COMMON_GBUFFER_3_VIEW");


    rtCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    rtCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    s_colorRT.Create(rtCreateInfo).SetDebugName("COMMON_COLOR_RT");
    s_colorRTView.Create(s_colorRT, mapping, subresourceRange).SetDebugName("COMMON_COLOR_RT");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_NONE,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_NONE,
            s_colorRT.Get(),
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    });


    rtCreateInfo.format = VK_FORMAT_D32_SFLOAT;
    rtCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    s_commonDepthRT.Create(rtCreateInfo).SetDebugName("COMMON_DEPTH_RT");
    s_commonDepthRTView.Create(s_commonDepthRT, mapping, subresourceRange).SetDebugName("COMMON_DEPTH_RT_VIEW");
}


static void DestroyDynamicRenderTargets()
{
    for (size_t i = 0; i < GBUFFER_RT_COUNT; ++i) {
        s_gbufferRTViews[i].Destroy();
        s_gbufferRTs[i].Destroy();
    }

    s_commonDepthRTView.Destroy();
    s_commonDepthRT.Destroy();

    s_colorRTView.Destroy();
    s_colorRT.Destroy();
}


static void ResizeDynamicRenderTargets()
{
    DestroyDynamicRenderTargets();
    CreateDynamicRenderTargets();
}


static void GenerateTextureMipmaps(vkn::CmdBuffer& cmdBuffer, vkn::Texture& texture, const TextureLoadData& loadData, uint32_t layerIdx = 0)
{
    CORE_ASSERT(layerIdx < texture.GetLayersCount());

    int32_t mipWidth  = texture.GetSizeX();
    int32_t mipHeight = texture.GetSizeY();

    for (uint32_t mip = 1; mip < loadData.GetMipsCount(); ++mip) {
        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_TRANSFER_READ_BIT,
            texture.Get(),
            VK_IMAGE_ASPECT_COLOR_BIT,
            mip - 1,
            1,
            layerIdx,
            1
        );

        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            texture.Get(),
            VK_IMAGE_ASPECT_COLOR_BIT,
            mip,
            1,
            layerIdx,
            1
        );

        VkImageBlit blit = {};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = mip - 1;
        blit.srcSubresource.baseArrayLayer = layerIdx;
        blit.srcSubresource.layerCount = 1;
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };

        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = mip;
        blit.dstSubresource.baseArrayLayer = layerIdx;
        blit.dstSubresource.layerCount = 1;
        blit.dstOffsets[1] = {
            mipWidth  > 1 ? mipWidth  / 2 : 1,
            mipHeight > 1 ? mipHeight / 2 : 1,
            1
        };

        vkCmdBlitImage(cmdBuffer.Get(),
            texture.Get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            texture.Get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR
        );

        if (mipWidth > 1) {
            mipWidth /= 2;
        }

        if (mipHeight > 1) {
            mipHeight /= 2;
        }
    }

    // Add this barrier to get all mips in same VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL layout
    CmdPipelineImageBarrier(
        cmdBuffer,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT,
        texture.Get(),
        VK_IMAGE_ASPECT_COLOR_BIT,
        loadData.GetMipsCount() - 1,
        1,
        layerIdx,
        1
    );
}


static void CreateSkybox(std::span<fs::path> faceDataPaths)
{
    Timer timer;

    CORE_ASSERT(faceDataPaths.size() == CUBEMAP_FACE_COUNT);

    std::array<TextureLoadData, CUBEMAP_FACE_COUNT> faceLoadDatas = {};
    for (size_t i = 0; i < faceDataPaths.size(); ++i) {
        faceLoadDatas[i].Load(faceDataPaths[i]);
        CORE_ASSERT_MSG(faceLoadDatas[i].IsLoaded(), "Skybox face \'%s\' data is not loaded", faceLoadDatas[i].GetName());
    }

    const uint32_t faceWidth = faceLoadDatas[0].GetWidth();
    const uint32_t faceHeight = faceLoadDatas[0].GetHeight();
    const uint32_t mipsCount = faceLoadDatas[0].GetMipsCount();
    const VkFormat format = faceLoadDatas[0].GetFormat();

#ifdef ENG_ASSERT_ENABLED
    for (const TextureLoadData& data : faceLoadDatas) {
        CORE_ASSERT_MSG(faceWidth == data.GetWidth(), "Skybox face \'%s\' width is not the same as others", data.GetName());
        CORE_ASSERT_MSG(faceHeight == data.GetHeight(), "Skybox face \'%s\' width is not the same as others", data.GetName());
        CORE_ASSERT_MSG(format == data.GetFormat(), "Skybox face \'%s\' format is not the same as others", data.GetName());
        CORE_ASSERT_MSG(mipsCount == data.GetMipsCount(), "Skybox face \'%s\' mip count is not the same as others", data.GetName());
    }
#endif

    vkn::AllocationInfo allocInfo = {};
    allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::TextureCreateInfo createInfo = {};
    createInfo.pDevice = &s_vkDevice;
    createInfo.type = VK_IMAGE_TYPE_2D;
    createInfo.extent = { faceWidth, faceHeight, 1 };
    createInfo.format = format;
    createInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT; 
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    createInfo.mipLevels = mipsCount;
    createInfo.arrayLayers = CUBEMAP_FACE_COUNT;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.pAllocInfo = &allocInfo;

    s_skyboxTexture.Create(createInfo).SetDebugName("COMMON_SKY_BOX");
    
    vkn::TextureViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.pOwner = &s_skyboxTexture;
    viewCreateInfo.type = VK_IMAGE_VIEW_TYPE_CUBE;
    viewCreateInfo.format = format;
    viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    s_skyboxTextureView.Create(viewCreateInfo).SetDebugName("COMMON_SKY_BOX_VIEW");

    for (size_t i = 0; i < CUBEMAP_FACE_COUNT; i += s_commonStagingBuffers.size()) {
        for (size_t j = 0; j < s_commonStagingBuffers.size() && i < CUBEMAP_FACE_COUNT; ++j) {
            if (i + j >= CUBEMAP_FACE_COUNT) {
                break;
            }
            
            vkn::Buffer& stagingBuffer = s_commonStagingBuffers[j];

            const TextureLoadData& loadData = faceLoadDatas[i + j];

            void* pData = stagingBuffer.Map<uint8_t>();
            memcpy(pData, loadData.GetData(), loadData.GetMemorySize());
            stagingBuffer.Unmap();
        }

        ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
            for (size_t j = 0; j < s_commonStagingBuffers.size(); ++j) {
                const uint32_t faceIdx = i + j;
                
                if (faceIdx >= CUBEMAP_FACE_COUNT) {
                    break;
                }

                vkn::Buffer& stagingBuffer = s_commonStagingBuffers[j];

                CmdPipelineImageBarrier(
                    cmdBuffer,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_2_NONE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_NONE,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    s_skyboxTexture.Get(),
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    0,
                    1,
                    faceIdx,
                    1
                );

                VkCopyBufferToImageInfo2 copyInfo = {};

                copyInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
                copyInfo.srcBuffer = stagingBuffer.Get();
                copyInfo.dstImage = s_skyboxTexture.Get();
                copyInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                copyInfo.regionCount = 1;

                VkBufferImageCopy2 texRegion = {};

                texRegion.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
                texRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                texRegion.imageSubresource.mipLevel = 0;
                texRegion.imageSubresource.baseArrayLayer = faceIdx;
                texRegion.imageSubresource.layerCount = 1;
                texRegion.imageExtent = s_skyboxTexture.GetSize();

                copyInfo.pRegions = &texRegion;

                vkCmdCopyBufferToImage2(cmdBuffer.Get(), &copyInfo);
            }
        });
    }

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        for (uint32_t layerIdx = 0; layerIdx < s_skyboxTexture.GetLayersCount(); ++layerIdx) {
            GenerateTextureMipmaps(cmdBuffer, s_skyboxTexture, faceLoadDatas[layerIdx], layerIdx);
        }

        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_READ_BIT,
            s_skyboxTexture.Get(),
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    });

    CORE_LOG_INFO("Skybox loading finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void CreateIBLResources()
{
    vkn::AllocationInfo allocInfo = {};
    allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    {
        vkn::TextureCreateInfo createInfo = {};
        createInfo.pDevice = &s_vkDevice;
        createInfo.type = VK_IMAGE_TYPE_2D;
        createInfo.extent = { COMMON_IRRADIANCE_MAP_SIZE.x, COMMON_IRRADIANCE_MAP_SIZE.y, 1 };
        createInfo.format = s_skyboxTexture.GetFormat();
        createInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT; 
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        createInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        createInfo.mipLevels = 1;
        createInfo.arrayLayers = CUBEMAP_FACE_COUNT;
        createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        createInfo.pAllocInfo = &allocInfo;
    
        s_irradianceMapTexture.Create(createInfo).SetDebugName("COMMON_IRRADIANCE_MAP");
    }

    {
        vkn::TextureCreateInfo createInfo = {};
        createInfo.pDevice = &s_vkDevice;
        createInfo.type = VK_IMAGE_TYPE_2D;
        createInfo.extent = { COMMON_PREFILTERED_ENV_MAP_SIZE.x, COMMON_PREFILTERED_ENV_MAP_SIZE.y, 1 };
        createInfo.format = s_skyboxTexture.GetFormat();
        createInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT; 
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        createInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        createInfo.mipLevels = COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT;
        createInfo.arrayLayers = CUBEMAP_FACE_COUNT;
        createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        createInfo.pAllocInfo = &allocInfo;
        
        s_prefilteredEnvMapTexture.Create(createInfo).SetDebugName("COMMON_PREFILTERED_ENV_MAP");
    }

    {
        vkn::TextureCreateInfo createInfo = {};
        createInfo.pDevice = &s_vkDevice;
        createInfo.type = VK_IMAGE_TYPE_2D;
        createInfo.extent = { COMMON_BRDF_INTEGRATION_LUT_SIZE.x, COMMON_BRDF_INTEGRATION_LUT_SIZE.y, 1 };
        createInfo.format = VK_FORMAT_R16G16_SFLOAT;
        createInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        createInfo.mipLevels = 1;
        createInfo.arrayLayers = 1;
        createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        createInfo.pAllocInfo = &allocInfo;

        s_brdfLUTTexture.Create(createInfo).SetDebugName("COMMON_BRDF_LUT");
    }

    {
        vkn::TextureViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.pOwner = &s_irradianceMapTexture;
        viewCreateInfo.type = VK_IMAGE_VIEW_TYPE_CUBE;
        viewCreateInfo.format = s_skyboxTexture.GetFormat();
        viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCreateInfo.subresourceRange.baseMipLevel = 0;
        viewCreateInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    
        s_irradianceMapTextureView.Create(viewCreateInfo).SetDebugName("COMMON_IRRADIANCE_MAP_VIEW");
    }

    {
        vkn::TextureViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.pOwner = &s_irradianceMapTexture;
        viewCreateInfo.type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewCreateInfo.format = s_skyboxTexture.GetFormat();
        viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCreateInfo.subresourceRange.baseMipLevel = 0;
        viewCreateInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        
        s_irradianceMapTextureViewRW.Create(viewCreateInfo).SetDebugName("COMMON_IRRADIANCE_MAP_VIEW_RW");
    }

    {
        vkn::TextureViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.pOwner = &s_prefilteredEnvMapTexture;
        viewCreateInfo.type = VK_IMAGE_VIEW_TYPE_CUBE;
        viewCreateInfo.format = s_skyboxTexture.GetFormat();
        viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCreateInfo.subresourceRange.baseMipLevel = 0;
        viewCreateInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        
        s_prefilteredEnvMapTextureView.Create(viewCreateInfo).SetDebugName("COMMON_PREFILTERED_ENV_MAP_VIEW");
    }
    
    {
        vkn::TextureViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.pOwner = &s_prefilteredEnvMapTexture;
        viewCreateInfo.type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewCreateInfo.format = s_skyboxTexture.GetFormat();
        viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        for (size_t mip = 0; mip < COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT; ++mip) {
            viewCreateInfo.subresourceRange.baseMipLevel = mip;
            viewCreateInfo.subresourceRange.levelCount = 1;

            s_prefilteredEnvMapTextureViewRWs[mip].Create(viewCreateInfo).SetDebugName("COMMON_PREFILTERED_ENV_MAP_VIEW_RW_%zu", mip);
        }
    }

    {
        vkn::TextureViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.pOwner = &s_brdfLUTTexture;
        viewCreateInfo.type = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = s_brdfLUTTexture.GetFormat();
        viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCreateInfo.subresourceRange.baseMipLevel = 0;
        viewCreateInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        
        s_brdfLUTTextureView.Create(viewCreateInfo).SetDebugName("COMMON_BRDF_LUT_VIEW");
    }

    {
        vkn::TextureViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.pOwner = &s_brdfLUTTexture;
        viewCreateInfo.type = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = s_brdfLUTTexture.GetFormat();
        viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCreateInfo.subresourceRange.baseMipLevel = 0;
        viewCreateInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        
        s_brdfLUTTextureViewRW.Create(viewCreateInfo).SetDebugName("COMMON_BRDF_LUT_VIEW_RW");
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
        .SetMaxDescriptorSetsCount(25);
        
    s_commonDescriptorSetPool = builder
        .AddResource(VK_DESCRIPTOR_TYPE_SAMPLER, (uint32_t)COMMON_SAMPLER_IDX::COUNT)
        .AddResource(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100)
        .AddResource(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100)
        .AddResource(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100)
        .AddResource(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000)
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
        .AddBinding(ZPASS_OPAQUE_INST_INFO_IDS_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .AddBinding(ZPASS_AKILL_INST_INFO_IDS_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build();

    CORE_ASSERT(s_zpassDescriptorSetLayout != VK_NULL_HANDLE);
}


static void CreateMeshCullingDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutBuilder builder;

    s_meshCullingDescriptorSetLayout = builder
        // .SetFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT)
        .AddBinding(MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(MESH_CULL_OPAQUE_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(MESH_CULL_AKILL_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(MESH_CULL_TRANSP_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .Build();

    CORE_ASSERT(s_meshCullingDescriptorSetLayout != VK_NULL_HANDLE);
}


static void CreateGBufferDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutBuilder builder;

    s_gbufferRenderDescriptorSetLayout = builder
        // .SetFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT)
        .AddBinding(GBUFFER_OPAQUE_INST_INFO_IDS_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .AddBinding(GBUFFER_AKILL_INST_INFO_IDS_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build();

    CORE_ASSERT(s_gbufferRenderDescriptorSetLayout != VK_NULL_HANDLE);
}


static void CreateDeferredLightingDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutBuilder builder;

    s_deferredLightingDescriptorSetLayout = builder
        // .SetFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT)
        .AddBinding(DEFERRED_LIGHTING_OUTPUT_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(DEFERRED_LIGHTING_GBUFFER_0_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(DEFERRED_LIGHTING_GBUFFER_1_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(DEFERRED_LIGHTING_GBUFFER_2_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(DEFERRED_LIGHTING_GBUFFER_3_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(DEFERRED_LIGHTING_DEPTH_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(DEFERRED_LIGHTING_IRRADIANCE_MAP_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(DEFERRED_LIGHTING_PREFILTERED_ENV_MAP_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(DEFERRED_LIGHTING_BRDF_LUT_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .Build();

    CORE_ASSERT(s_deferredLightingDescriptorSetLayout != VK_NULL_HANDLE);
}


static void CreatePostProcessingDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutBuilder builder;

    s_postProcessingDescriptorSetLayout = builder
        // .SetFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT)
        .AddBinding(POST_PROCESSING_INPUT_COLOR_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build();

    CORE_ASSERT(s_postProcessingDescriptorSetLayout != VK_NULL_HANDLE);
}


static void CreateSkyboxDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutBuilder builder;

    s_skyboxDescriptorSetLayout = builder
        // .SetFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT)
        .AddBinding(SKYBOX_TEXTURE_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build();

    CORE_ASSERT(s_skyboxDescriptorSetLayout != VK_NULL_HANDLE);
}


static void CreateIrradianceMapGenDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutBuilder builder;

    s_irradianceMapGenDescriptorSetLayout = builder
        // .SetFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT)
        .AddBinding(IRRADIANCE_MAP_GEN_ENV_MAP_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(IRRADIANCE_MAP_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .Build();

    CORE_ASSERT(s_irradianceMapGenDescriptorSetLayout != VK_NULL_HANDLE);
}


static void CreatePrefilteredEnvMapGenDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutBuilder builder;

    s_prefilteredEnvMapGenDescriptorSetLayout = builder
        // .SetFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT)
        .AddBinding(PREFILTERED_ENV_MAP_GEN_ENV_MAP_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .AddBinding(PREFILTERED_ENV_MAP_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .Build();

    CORE_ASSERT(s_prefilteredEnvMapGenDescriptorSetLayout != VK_NULL_HANDLE);
}


static void CreateBRDFIntegrationLUTGenDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutBuilder builder;

    s_BRDFIntegrationLUTGenDescriptorSetLayout = builder
        // .SetFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT)
        .AddBinding(BRDF_INTEGRATION_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT)
        .Build();

    CORE_ASSERT(s_BRDFIntegrationLUTGenDescriptorSetLayout != VK_NULL_HANDLE);
}


static void AllocateDescriptorSets()
{
    vkn::DescriptorSetAllocator allocator;

    std::vector descriptorSetsPairs = {
        std::make_pair(&s_commonDescriptorSetLayout,                &s_commonDescriptorSet),
        std::make_pair(&s_meshCullingDescriptorSetLayout,           &s_meshCullingDescriptorSet),
        std::make_pair(&s_zpassDescriptorSetLayout,                 &s_zpassDescriptorSet),
        std::make_pair(&s_gbufferRenderDescriptorSetLayout,         &s_gbufferRenderDescriptorSet),
        std::make_pair(&s_deferredLightingDescriptorSetLayout,      &s_deferredLightingDescriptorSet),
        std::make_pair(&s_skyboxDescriptorSetLayout,                &s_skyboxDescriptorSet),
        std::make_pair(&s_postProcessingDescriptorSetLayout,        &s_postProcessingDescriptorSet),
        std::make_pair(&s_irradianceMapGenDescriptorSetLayout,      &s_irradianceMapGenDescriptorSet),
        std::make_pair(&s_BRDFIntegrationLUTGenDescriptorSetLayout, &s_BRDFIntegrationLUTGenDescriptorSet),
    };

    for (size_t i = 0; i < s_prefilteredEnvGenDescriptorSets.size(); ++i) {
        descriptorSetsPairs.emplace_back(std::make_pair(&s_prefilteredEnvMapGenDescriptorSetLayout, &s_prefilteredEnvGenDescriptorSets[i]));
    }

    std::vector<VkDescriptorSet> descriptorSets(descriptorSetsPairs.size());

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
    CreateDeferredLightingDescriptorSetLayout();
    CreatePostProcessingDescriptorSetLayout();
    CreateSkyboxDescriptorSetLayout();
    CreateIrradianceMapGenDescriptorSetLayout();
    CreatePrefilteredEnvMapGenDescriptorSetLayout();
    CreateBRDFIntegrationLUTGenDescriptorSetLayout();

    AllocateDescriptorSets();
}


static void CreateMeshCullingPipelineLayout()
{
    vkn::PipelineLayoutBuilder builder(s_vkPhysDevice.GetProperties().limits.maxPushConstantsSize);

    s_meshCullingPipelineLayout = builder
        .AddPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MESH_CULLING_PUSH_CONSTS))
        .AddDescriptorSetLayout(s_commonDescriptorSetLayout)
        .AddDescriptorSetLayout(s_meshCullingDescriptorSetLayout)
        .Build();

    CORE_ASSERT(s_meshCullingPipelineLayout != VK_NULL_HANDLE);
}


static void CreateZPassPipelineLayout()
{
    vkn::PipelineLayoutBuilder builder(s_vkPhysDevice.GetProperties().limits.maxPushConstantsSize);

    s_zpassPipelineLayout = builder
        .AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ZPASS_PUSH_CONSTS))
        .AddDescriptorSetLayout(s_commonDescriptorSetLayout)
        .AddDescriptorSetLayout(s_zpassDescriptorSetLayout)
        .Build();

    CORE_ASSERT(s_zpassPipelineLayout != VK_NULL_HANDLE);
}


static void CreateGBufferPipelineLayout()
{
    vkn::PipelineLayoutBuilder builder(s_vkPhysDevice.GetProperties().limits.maxPushConstantsSize);

    s_gbufferRenderPipelineLayout = builder
        .AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GBUFFER_PUSH_CONSTS))
        .AddDescriptorSetLayout(s_commonDescriptorSetLayout)
        .AddDescriptorSetLayout(s_gbufferRenderDescriptorSetLayout)
        .Build();

    CORE_ASSERT(s_gbufferRenderPipelineLayout != VK_NULL_HANDLE);
}


static void CreateDeferredLightingPipelineLayout()
{
    vkn::PipelineLayoutBuilder builder(s_vkPhysDevice.GetProperties().limits.maxPushConstantsSize);

    s_deferredLightingPipelineLayout = builder
        .AddDescriptorSetLayout(s_commonDescriptorSetLayout)
        .AddDescriptorSetLayout(s_deferredLightingDescriptorSetLayout)
        .Build();

    CORE_ASSERT(s_deferredLightingPipelineLayout != VK_NULL_HANDLE);
}


static void CreatePostProcessingPipelineLayout()
{
    vkn::PipelineLayoutBuilder builder(s_vkPhysDevice.GetProperties().limits.maxPushConstantsSize);

    s_postProcessingPipelineLayout = builder
        .AddDescriptorSetLayout(s_commonDescriptorSetLayout)
        .AddDescriptorSetLayout(s_postProcessingDescriptorSetLayout)
        .Build();

    CORE_ASSERT(s_postProcessingPipelineLayout != VK_NULL_HANDLE);
}


static void CreateSkyboxPipelineLayout()
{
    vkn::PipelineLayoutBuilder builder(s_vkPhysDevice.GetProperties().limits.maxPushConstantsSize);

    s_skyboxPipelineLayout = builder
        .AddDescriptorSetLayout(s_commonDescriptorSetLayout)
        .AddDescriptorSetLayout(s_skyboxDescriptorSetLayout)
        .Build();

    CORE_ASSERT(s_skyboxPipelineLayout != VK_NULL_HANDLE);
}


static void CreateIrradianceMapGenPipelineLayout()
{
    vkn::PipelineLayoutBuilder builder(s_vkPhysDevice.GetProperties().limits.maxPushConstantsSize);

    s_irradianceMapGenPipelineLayout = builder
        .AddPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(IRRADIANCE_MAP_PUSH_CONSTS))
        .AddDescriptorSetLayout(s_commonDescriptorSetLayout)
        .AddDescriptorSetLayout(s_irradianceMapGenDescriptorSetLayout)
        .Build();

    CORE_ASSERT(s_irradianceMapGenPipelineLayout != VK_NULL_HANDLE);
}


static void CreatePrefilteredEnvMapGenPipelineLayout()
{
    vkn::PipelineLayoutBuilder builder(s_vkPhysDevice.GetProperties().limits.maxPushConstantsSize);

    s_prefilteredEnvMapGenPipelineLayout = builder
        .AddPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PREFILTERED_ENV_MAP_PUSH_CONSTS))
        .AddDescriptorSetLayout(s_commonDescriptorSetLayout)
        .AddDescriptorSetLayout(s_prefilteredEnvMapGenDescriptorSetLayout)
        .Build();

    CORE_ASSERT(s_prefilteredEnvMapGenPipelineLayout != VK_NULL_HANDLE);
}


static void CreateBRDFIntegrationLUTGenPipelineLayout()
{
    vkn::PipelineLayoutBuilder builder(s_vkPhysDevice.GetProperties().limits.maxPushConstantsSize);

    s_BRDFIntegrationLUTGenPipelineLayout = builder
        .AddPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BRDF_INTEGRATION_PUSH_CONSTS))
        .AddDescriptorSetLayout(s_commonDescriptorSetLayout)
        .AddDescriptorSetLayout(s_BRDFIntegrationLUTGenDescriptorSetLayout)
        .Build();

    CORE_ASSERT(s_BRDFIntegrationLUTGenPipelineLayout != VK_NULL_HANDLE);
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

    builder.SetDepthAttachmentFormat(s_commonDepthRT.GetFormat());
    
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
        .SetDepthTestState(VK_TRUE, VK_FALSE, VK_COMPARE_OP_EQUAL)
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

    for (const vkn::Texture& colorRT : s_gbufferRTs) {
        builder.AddColorAttachmentFormat(colorRT.GetFormat());  
        builder.AddColorBlendAttachment(blendState);   
    }
    builder.SetDepthAttachmentFormat(s_commonDepthRT.GetFormat());
    
    s_gbufferRenderPipeline = builder.Build();

    for (VkShaderModule& shader : shaderModules) {
        vkDestroyShaderModule(s_vkDevice.Get(), shader, nullptr);
        shader = VK_NULL_HANDLE;
    }

    CORE_ASSERT(s_gbufferRenderPipeline != VK_NULL_HANDLE);
}


static void CreateDeferredLightingPipeline(const fs::path& csPath)
{
    std::vector<uint8_t> shaderCodeBuffer;
    VkShaderModule shaderModule = CreateVkShaderModule(csPath, &shaderCodeBuffer);

    vkn::ComputePipelineBuilder builder;

    s_deferredLightingPipeline = builder
        .SetShader(shaderModule, "main")
        .SetLayout(s_deferredLightingPipelineLayout)
        .Build();
    
    vkDestroyShaderModule(s_vkDevice.Get(), shaderModule, nullptr);
    shaderModule = VK_NULL_HANDLE;

    CORE_ASSERT(s_deferredLightingPipeline != VK_NULL_HANDLE);
}


static void CreatePostProcessingPipeline(const fs::path& vsPath, const fs::path& psPath)
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

    VkPipelineColorBlendAttachmentState blendState = {};
    blendState.blendEnable = VK_FALSE;
    blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    
    s_postProcessingPipeline = builder
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetStencilTestState(VK_FALSE, {}, {})
        .SetDepthTestState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_EQUAL)
        .SetDepthBoundsTestState(VK_FALSE, 0.f, 1.f)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .SetRasterizerLineWidth(1.f)
        .AddColorAttachmentFormat(s_vkSwapchain.GetImageFormat())
        .AddColorBlendAttachment(blendState)
        .SetLayout(s_postProcessingPipelineLayout)
        .Build();

    for (VkShaderModule& shader : shaderModules) {
        vkDestroyShaderModule(s_vkDevice.Get(), shader, nullptr);
        shader = VK_NULL_HANDLE;
    }

    CORE_ASSERT(s_postProcessingPipeline != VK_NULL_HANDLE);
}


static void CreateSkyboxPipeline(const fs::path& vsPath, const fs::path& psPath)
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

    VkPipelineColorBlendAttachmentState blendState = {};
    blendState.blendEnable = VK_FALSE;
    blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    
    s_skyboxPipeline = builder
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_NONE)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetStencilTestState(VK_FALSE, {}, {})
    #ifdef ENG_REVERSED_Z
        .SetDepthTestState(VK_TRUE, VK_FALSE, VK_COMPARE_OP_GREATER_OR_EQUAL)
    #else
        .SetDepthTestState(VK_TRUE, VK_FALSE VK_COMPARE_OP_LESS_OR_EQUAL)
    #endif
        .SetDepthBoundsTestState(VK_TRUE, 0.f, 1.f)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .SetRasterizerLineWidth(1.f)
        .AddColorAttachmentFormat(s_colorRT.GetFormat())
        .AddColorBlendAttachment(blendState)
        .SetDepthAttachmentFormat(s_commonDepthRT.GetFormat())
        .SetLayout(s_skyboxPipelineLayout)
        .Build();

    for (VkShaderModule& shader : shaderModules) {
        vkDestroyShaderModule(s_vkDevice.Get(), shader, nullptr);
        shader = VK_NULL_HANDLE;
    }

    CORE_ASSERT(s_skyboxPipeline != VK_NULL_HANDLE);
}


static void CreateIrradianceMapGenPipeline(const fs::path& csPath)
{
    std::vector<uint8_t> shaderCodeBuffer;
    VkShaderModule shaderModule = CreateVkShaderModule(csPath, &shaderCodeBuffer);

    vkn::ComputePipelineBuilder builder;

    s_irradianceMapGenPipeline = builder
        .SetShader(shaderModule, "main")
        .SetLayout(s_irradianceMapGenPipelineLayout)
        .Build();
    
    vkDestroyShaderModule(s_vkDevice.Get(), shaderModule, nullptr);
    shaderModule = VK_NULL_HANDLE;

    CORE_ASSERT(s_irradianceMapGenPipeline != VK_NULL_HANDLE);
}


static void CreatePrefilteredEnvMapGenPipeline(const fs::path& csPath)
{
    std::vector<uint8_t> shaderCodeBuffer;
    VkShaderModule shaderModule = CreateVkShaderModule(csPath, &shaderCodeBuffer);

    vkn::ComputePipelineBuilder builder;

    s_prefilteredEnvMapGenPipeline = builder
        .SetShader(shaderModule, "main")
        .SetLayout(s_prefilteredEnvMapGenPipelineLayout)
        .Build();
    
    vkDestroyShaderModule(s_vkDevice.Get(), shaderModule, nullptr);
    shaderModule = VK_NULL_HANDLE;

    CORE_ASSERT(s_prefilteredEnvMapGenPipeline != VK_NULL_HANDLE);
}


static void CreateBRDFIntegrationLUTGenPipeline(const fs::path& csPath)
{
    std::vector<uint8_t> shaderCodeBuffer;
    VkShaderModule shaderModule = CreateVkShaderModule(csPath, &shaderCodeBuffer);

    vkn::ComputePipelineBuilder builder;

    s_BRDFIntegrationLUTGenPipeline = builder
        .SetShader(shaderModule, "main")
        .SetLayout(s_BRDFIntegrationLUTGenPipelineLayout)
        .Build();
    
    vkDestroyShaderModule(s_vkDevice.Get(), shaderModule, nullptr);
    shaderModule = VK_NULL_HANDLE;

    CORE_ASSERT(s_BRDFIntegrationLUTGenPipeline != VK_NULL_HANDLE);
}


static void CreatePipelines()
{
    CreateMeshCullingPipelineLayout();
    CreateZPassPipelineLayout();
    CreateGBufferPipelineLayout();
    CreateDeferredLightingPipelineLayout();
    CreatePostProcessingPipelineLayout();
    CreateSkyboxPipelineLayout();
    CreateIrradianceMapGenPipelineLayout();
    CreatePrefilteredEnvMapGenPipelineLayout();
    CreateBRDFIntegrationLUTGenPipelineLayout();
    CreateMeshCullingPipeline("shaders/bin/mesh_culling.cs.spv");
    CreateZPassPipeline("shaders/bin/zpass.vs.spv", "shaders/bin/zpass.ps.spv");
    CreateGBufferRenderPipeline("shaders/bin/gbuffer.vs.spv", "shaders/bin/gbuffer.ps.spv");
    CreateDeferredLightingPipeline("shaders/bin/deferred_lighting.cs.spv");
    CreatePostProcessingPipeline("shaders/bin/post_processing.vs.spv", "shaders/bin/post_processing.ps.spv");
    CreateSkyboxPipeline("shaders/bin/skybox.vs.spv", "shaders/bin/skybox.ps.spv");
    CreateIrradianceMapGenPipeline("shaders/bin/irradiance_map_gen.cs.spv");
    CreatePrefilteredEnvMapGenPipeline("shaders/bin/prefiltered_env_map_gen.cs.spv");
    CreateBRDFIntegrationLUTGenPipeline("shaders/bin/brdf_integration_gen.cs.spv");
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


static void CreateCullingResources()
{
    vkn::AllocationInfo allocInfo = {};
    allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo createInfo = {};
    createInfo.pDevice = &s_vkDevice;
    createInfo.size = MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(COMMON_INDIRECT_DRAW_CMD);
    createInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    createInfo.pAllocInfo = &allocInfo;

    s_commonOpaqueMeshDrawCmdBuffer.Create(createInfo).SetDebugName("COMMON_OPAQUE_MESH_DRAW_CMD_BUFFER");

    createInfo.size = sizeof(glm::uint);

    s_commonOpaqueMeshDrawCmdCountBuffer.Create(createInfo).SetDebugName("COMMON_OPAQUE_MESH_DRAW_CMD_COUNT_BUFFER");

    createInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    createInfo.size = MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(glm::uint);
    
    s_commonCulledOpaqueInstInfoIDsBuffer.Create(createInfo).SetDebugName("COMMON_CULLED_OPAQUE_INST_INFO_IDS_BUFFER");


    createInfo.size = MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(COMMON_INDIRECT_DRAW_CMD);
    createInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

    s_commonAKillMeshDrawCmdBuffer.Create(createInfo).SetDebugName("COMMON_AKILL_MESH_DRAW_CMD_BUFFER");

    createInfo.size = sizeof(glm::uint);

    s_commonAKillMeshDrawCmdCountBuffer.Create(createInfo).SetDebugName("COMMON_AKILL_MESH_DRAW_CMD_COUNT_BUFFER");

    createInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    createInfo.size = MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(glm::uint);
    
    s_commonCulledAKillInstInfoIDsBuffer.Create(createInfo).SetDebugName("COMMON_CULLED_AKILL_INST_INFO_IDS_BUFFER");


    createInfo.size = MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(COMMON_INDIRECT_DRAW_CMD);
    createInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

    s_commonTranspMeshDrawCmdBuffer.Create(createInfo).SetDebugName("COMMON_TRANSP_MESH_DRAW_CMD_BUFFER");

    createInfo.size = sizeof(glm::uint);

    s_commonTranspMeshDrawCmdCountBuffer.Create(createInfo).SetDebugName("COMMON_TRANSP_MESH_DRAW_CMD_COUNT_BUFFER");

    createInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    createInfo.size = MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(glm::uint);
    
    s_commonCulledTranspInstInfoIDsBuffer.Create(createInfo).SetDebugName("COMMON_CULLED_TRANSP_INST_INFO_IDS_BUFFER");
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


static void WriteZPassDescriptorSet()
{
    std::vector<VkWriteDescriptorSet> descWrites;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = s_zpassDescriptorSet;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    VkDescriptorBufferInfo culledOpaqueInstInfosIDsBuffInfo = {};
    culledOpaqueInstInfosIDsBuffInfo.buffer = s_commonCulledOpaqueInstInfoIDsBuffer.Get();
    culledOpaqueInstInfosIDsBuffInfo.offset = 0;
    culledOpaqueInstInfosIDsBuffInfo.range = VK_WHOLE_SIZE;
    
    write.dstBinding = ZPASS_OPAQUE_INST_INFO_IDS_DESCRIPTOR_SLOT;
    write.pBufferInfo = &culledOpaqueInstInfosIDsBuffInfo;

    descWrites.emplace_back(write);


    VkDescriptorBufferInfo culledAKillInstInfosIDsBuffInfo = {};
    culledAKillInstInfosIDsBuffInfo.buffer = s_commonCulledAKillInstInfoIDsBuffer.Get();
    culledAKillInstInfosIDsBuffInfo.offset = 0;
    culledAKillInstInfosIDsBuffInfo.range = VK_WHOLE_SIZE;
    
    write.dstBinding = ZPASS_AKILL_INST_INFO_IDS_DESCRIPTOR_SLOT;
    write.pBufferInfo = &culledAKillInstInfosIDsBuffInfo;

    descWrites.emplace_back(write);


    vkUpdateDescriptorSets(s_vkDevice.Get(), descWrites.size(), descWrites.data(), 0, nullptr);
}


static void WriteMeshCullingDescriptorSet()
{
    std::vector<VkWriteDescriptorSet> descWrites;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = s_meshCullingDescriptorSet;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    VkDescriptorBufferInfo opaqueMeshDrawCmdBufferInfo = {};
    opaqueMeshDrawCmdBufferInfo.buffer = s_commonOpaqueMeshDrawCmdBuffer.Get();
    opaqueMeshDrawCmdBufferInfo.offset = 0;
    opaqueMeshDrawCmdBufferInfo.range = VK_WHOLE_SIZE;

    write.dstBinding = MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT;
    write.pBufferInfo = &opaqueMeshDrawCmdBufferInfo;

    descWrites.emplace_back(write);

    VkDescriptorBufferInfo akillMeshDrawCmdBufferInfo = {};
    akillMeshDrawCmdBufferInfo.buffer = s_commonAKillMeshDrawCmdBuffer.Get();
    akillMeshDrawCmdBufferInfo.offset = 0;
    akillMeshDrawCmdBufferInfo.range = VK_WHOLE_SIZE;

    write.dstBinding = MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT;
    write.pBufferInfo = &akillMeshDrawCmdBufferInfo;

    descWrites.emplace_back(write);

    VkDescriptorBufferInfo transpMeshDrawCmdBufferInfo = {};
    transpMeshDrawCmdBufferInfo.buffer = s_commonTranspMeshDrawCmdBuffer.Get();
    transpMeshDrawCmdBufferInfo.offset = 0;
    transpMeshDrawCmdBufferInfo.range = VK_WHOLE_SIZE;

    write.dstBinding = MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT;
    write.pBufferInfo = &transpMeshDrawCmdBufferInfo;

    descWrites.emplace_back(write);


    VkDescriptorBufferInfo opaqueMeshDrawCmdCountBufferInfo = {};
    opaqueMeshDrawCmdCountBufferInfo.buffer = s_commonOpaqueMeshDrawCmdCountBuffer.Get();
    opaqueMeshDrawCmdCountBufferInfo.offset = 0;
    opaqueMeshDrawCmdCountBufferInfo.range = VK_WHOLE_SIZE;

    write.dstBinding = MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT;
    write.pBufferInfo = &opaqueMeshDrawCmdCountBufferInfo;

    descWrites.emplace_back(write);

    VkDescriptorBufferInfo akillMeshDrawCmdCountBufferInfo = {};
    akillMeshDrawCmdCountBufferInfo.buffer = s_commonAKillMeshDrawCmdCountBuffer.Get();
    akillMeshDrawCmdCountBufferInfo.offset = 0;
    akillMeshDrawCmdCountBufferInfo.range = VK_WHOLE_SIZE;

    write.dstBinding = MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT;
    write.pBufferInfo = &akillMeshDrawCmdCountBufferInfo;

    descWrites.emplace_back(write);

    VkDescriptorBufferInfo transpMeshDrawCmdCountBufferInfo = {};
    transpMeshDrawCmdCountBufferInfo.buffer = s_commonTranspMeshDrawCmdCountBuffer.Get();
    transpMeshDrawCmdCountBufferInfo.offset = 0;
    transpMeshDrawCmdCountBufferInfo.range = VK_WHOLE_SIZE;

    write.dstBinding = MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_COUNT_UAV_DESCRIPTOR_SLOT;
    write.pBufferInfo = &transpMeshDrawCmdCountBufferInfo;

    descWrites.emplace_back(write);


    VkDescriptorBufferInfo culledOpaqueInstInfoIDsBufferInfo = {};
    culledOpaqueInstInfoIDsBufferInfo.buffer = s_commonCulledOpaqueInstInfoIDsBuffer.Get();
    culledOpaqueInstInfoIDsBufferInfo.offset = 0;
    culledOpaqueInstInfoIDsBufferInfo.range = VK_WHOLE_SIZE;

    write.dstBinding = MESH_CULL_OPAQUE_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT;
    write.pBufferInfo = &culledOpaqueInstInfoIDsBufferInfo;

    descWrites.emplace_back(write);    

    VkDescriptorBufferInfo culledAKillInstInfoIDsBufferInfo = {};
    culledAKillInstInfoIDsBufferInfo.buffer = s_commonCulledAKillInstInfoIDsBuffer.Get();
    culledAKillInstInfoIDsBufferInfo.offset = 0;
    culledAKillInstInfoIDsBufferInfo.range = VK_WHOLE_SIZE;

    write.dstBinding = MESH_CULL_AKILL_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT;
    write.pBufferInfo = &culledAKillInstInfoIDsBufferInfo;

    descWrites.emplace_back(write);

    VkDescriptorBufferInfo culledTranspInstInfoIDsBufferInfo = {};
    culledTranspInstInfoIDsBufferInfo.buffer = s_commonCulledTranspInstInfoIDsBuffer.Get();
    culledTranspInstInfoIDsBufferInfo.offset = 0;
    culledTranspInstInfoIDsBufferInfo.range = VK_WHOLE_SIZE;

    write.dstBinding = MESH_CULL_TRANSP_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT;
    write.pBufferInfo = &culledTranspInstInfoIDsBufferInfo;

    descWrites.emplace_back(write);


    vkUpdateDescriptorSets(s_vkDevice.Get(), descWrites.size(), descWrites.data(), 0, nullptr);
}


static void WriteGBufferDescriptorSet()
{
    std::vector<VkWriteDescriptorSet> descWrites;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = s_gbufferRenderDescriptorSet;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    
    VkDescriptorBufferInfo culledOpaqueInstInfoIDsBuffer = {};
    culledOpaqueInstInfoIDsBuffer.buffer = s_commonCulledOpaqueInstInfoIDsBuffer.Get();
    culledOpaqueInstInfoIDsBuffer.offset = 0;
    culledOpaqueInstInfoIDsBuffer.range = VK_WHOLE_SIZE;

    write.dstBinding = GBUFFER_OPAQUE_INST_INFO_IDS_DESCRIPTOR_SLOT;
    write.pBufferInfo = &culledOpaqueInstInfoIDsBuffer;

    descWrites.emplace_back(write);


    VkDescriptorBufferInfo culledAKillInstInfoIDsBuffer = {};
    culledAKillInstInfoIDsBuffer.buffer = s_commonCulledAKillInstInfoIDsBuffer.Get();
    culledAKillInstInfoIDsBuffer.offset = 0;
    culledAKillInstInfoIDsBuffer.range = VK_WHOLE_SIZE;

    write.dstBinding = GBUFFER_AKILL_INST_INFO_IDS_DESCRIPTOR_SLOT;
    write.pBufferInfo = &culledAKillInstInfoIDsBuffer;

    descWrites.emplace_back(write);


    vkUpdateDescriptorSets(s_vkDevice.Get(), descWrites.size(), descWrites.data(), 0, nullptr);
}


static void WriteDeferredLightingDescriptorSet()
{
    std::vector<VkWriteDescriptorSet> descWrites;

    VkDescriptorImageInfo lightingOutputWriteInfo = {};
    lightingOutputWriteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    lightingOutputWriteInfo.imageView = s_colorRTView.Get();

    VkWriteDescriptorSet lightingOutputWrite = {};
    lightingOutputWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    lightingOutputWrite.dstSet = s_deferredLightingDescriptorSet;
    lightingOutputWrite.dstBinding = DEFERRED_LIGHTING_OUTPUT_UAV_DESCRIPTOR_SLOT;
    lightingOutputWrite.dstArrayElement = 0;
    lightingOutputWrite.descriptorCount = 1;
    lightingOutputWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    lightingOutputWrite.pImageInfo = &lightingOutputWriteInfo;

    descWrites.emplace_back(lightingOutputWrite);

    std::array<VkDescriptorImageInfo, GBUFFER_RT_COUNT> gbufferInputImageInfos = {};
    for (size_t i = 0; i < GBUFFER_RT_COUNT; ++i) {
        VkDescriptorImageInfo& info = gbufferInputImageInfos[i];

        info.imageView = s_gbufferRTViews[i].Get();
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    for (size_t i = 0; i < GBUFFER_RT_COUNT; ++i) {
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = s_deferredLightingDescriptorSet;
        write.dstBinding = DEFERRED_LIGHTING_GBUFFER_0_DESCRIPTOR_SLOT + i;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo = &gbufferInputImageInfos[i];

        descWrites.emplace_back(write);        
    }

    VkDescriptorImageInfo depthImageInfo = {};
    depthImageInfo.imageView = s_commonDepthRTView.Get();
    depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet depthWrite = {};
    depthWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    depthWrite.dstSet = s_deferredLightingDescriptorSet;
    depthWrite.dstBinding = DEFERRED_LIGHTING_DEPTH_DESCRIPTOR_SLOT;
    depthWrite.dstArrayElement = 0;
    depthWrite.descriptorCount = 1;
    depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    depthWrite.pImageInfo = &depthImageInfo;

    descWrites.emplace_back(depthWrite);
    
    VkDescriptorImageInfo irradianceImageInfo = {};
    irradianceImageInfo.imageView = s_irradianceMapTextureView.Get();
    irradianceImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet irradianceWrite = {};
    irradianceWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    irradianceWrite.dstSet = s_deferredLightingDescriptorSet;
    irradianceWrite.dstBinding = DEFERRED_LIGHTING_IRRADIANCE_MAP_DESCRIPTOR_SLOT;
    irradianceWrite.dstArrayElement = 0;
    irradianceWrite.descriptorCount = 1;
    irradianceWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    irradianceWrite.pImageInfo = &irradianceImageInfo;

    descWrites.emplace_back(irradianceWrite);

    VkDescriptorImageInfo prefiltImageInfo = {};
    prefiltImageInfo.imageView = s_prefilteredEnvMapTextureView.Get();
    prefiltImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet prefiltImageWrite = {};
    prefiltImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    prefiltImageWrite.dstSet = s_deferredLightingDescriptorSet;
    prefiltImageWrite.dstBinding = DEFERRED_LIGHTING_PREFILTERED_ENV_MAP_DESCRIPTOR_SLOT;
    prefiltImageWrite.dstArrayElement = 0;
    prefiltImageWrite.descriptorCount = 1;
    prefiltImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    prefiltImageWrite.pImageInfo = &prefiltImageInfo;

    descWrites.emplace_back(prefiltImageWrite);

    VkDescriptorImageInfo brdfLUTInfo = {};
    brdfLUTInfo.imageView = s_brdfLUTTextureView.Get();
    brdfLUTInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet brdfLUTWrite = {};
    brdfLUTWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    brdfLUTWrite.dstSet = s_deferredLightingDescriptorSet;
    brdfLUTWrite.dstBinding = DEFERRED_LIGHTING_BRDF_LUT_DESCRIPTOR_SLOT;
    brdfLUTWrite.dstArrayElement = 0;
    brdfLUTWrite.descriptorCount = 1;
    brdfLUTWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    brdfLUTWrite.pImageInfo = &brdfLUTInfo;

    descWrites.emplace_back(brdfLUTWrite);

    vkUpdateDescriptorSets(s_vkDevice.Get(), descWrites.size(), descWrites.data(), 0, nullptr);
}


static void WritePostProcessingDescriptorSet()
{
    std::array<VkWriteDescriptorSet, 1> descWrites = {};

    VkDescriptorImageInfo postProcInputWriteInfo = {};
    postProcInputWriteInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    postProcInputWriteInfo.imageView = s_colorRTView.Get();

    VkWriteDescriptorSet postProcInputWrite = {};
    postProcInputWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    postProcInputWrite.dstSet = s_postProcessingDescriptorSet;
    postProcInputWrite.dstBinding = POST_PROCESSING_INPUT_COLOR_DESCRIPTOR_SLOT;
    postProcInputWrite.dstArrayElement = 0;
    postProcInputWrite.descriptorCount = 1;
    postProcInputWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    postProcInputWrite.pImageInfo = &postProcInputWriteInfo;

    descWrites[0] = postProcInputWrite;

    vkUpdateDescriptorSets(s_vkDevice.Get(), descWrites.size(), descWrites.data(), 0, nullptr);
}


static void WriteSkyboxDescriptorSet()
{
    std::array<VkWriteDescriptorSet, 1> descWrites = {};

    VkDescriptorImageInfo skyboxTexInfo = {};
    skyboxTexInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    skyboxTexInfo.imageView = s_skyboxTextureView.Get();
    // skyboxTexInfo.imageView = s_prefilteredEnvMapTextureView.Get();
    // skyboxTexInfo.imageView = s_irradianceMapTextureView.Get();

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = s_skyboxDescriptorSet;
    write.dstBinding = SKYBOX_TEXTURE_DESCRIPTOR_SLOT;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo = &skyboxTexInfo;

    descWrites[0] = write;

    vkUpdateDescriptorSets(s_vkDevice.Get(), descWrites.size(), descWrites.data(), 0, nullptr);
}


static void WriteIrradianceMapGenDescriptorSet()
{
    std::array<VkWriteDescriptorSet, 2> descWrites;

    VkDescriptorImageInfo envMapTexInfo = {};
    envMapTexInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    envMapTexInfo.imageView = s_skyboxTextureView.Get();

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = s_irradianceMapGenDescriptorSet;
    write.dstBinding = IRRADIANCE_MAP_GEN_ENV_MAP_DESCRIPTOR_SLOT;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo = &envMapTexInfo;

    descWrites[0] = write;

    VkDescriptorImageInfo irrMapTexInfo = {};
    irrMapTexInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    irrMapTexInfo.imageView = s_irradianceMapTextureViewRW.Get();

    write.dstBinding = IRRADIANCE_MAP_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo = &irrMapTexInfo;

    descWrites[1] = write;

    vkUpdateDescriptorSets(s_vkDevice.Get(), descWrites.size(), descWrites.data(), 0, nullptr);
}


static void WritePrefilteredEnvMapGenDescriptorSets()
{
    std::vector<VkWriteDescriptorSet> descWrites;

    VkDescriptorImageInfo envMapTexInfo = {};
    envMapTexInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    envMapTexInfo.imageView = s_skyboxTextureView.Get();

    std::array<VkDescriptorImageInfo, COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT> prefiltEnvMapMipsInfos = {};

    VkWriteDescriptorSet write = {};

    for (size_t mip = 0; mip < COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT; ++mip) {
        VkDescriptorSet set = s_prefilteredEnvGenDescriptorSets[mip];
        
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        
        write.dstBinding = PREFILTERED_ENV_MAP_GEN_ENV_MAP_DESCRIPTOR_SLOT;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo = &envMapTexInfo;

        descWrites.emplace_back(write);

        VkDescriptorImageInfo& mipInfo = prefiltEnvMapMipsInfos[mip];

        mipInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        mipInfo.imageView = s_prefilteredEnvMapTextureViewRWs[mip].Get();

        write.dstBinding = PREFILTERED_ENV_MAP_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.pImageInfo = &mipInfo;

        descWrites.emplace_back(write);
    }
    
    vkUpdateDescriptorSets(s_vkDevice.Get(), descWrites.size(), descWrites.data(), 0, nullptr);
}


static void WriteBRDFIntegrationLUTGenDescriptorSet()
{
    std::array<VkWriteDescriptorSet, 1> descWrites;

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.imageView = s_brdfLUTTextureViewRW.Get();

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = s_BRDFIntegrationLUTGenDescriptorSet;
    write.dstBinding = BRDF_INTEGRATION_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo = &imageInfo;

    descWrites[0] = write;

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
    WriteZPassDescriptorSet();
    WriteMeshCullingDescriptorSet();
    WriteGBufferDescriptorSet();
    WriteDeferredLightingDescriptorSet();
    WritePostProcessingDescriptorSet();
    WriteSkyboxDescriptorSet();
    WriteIrradianceMapGenDescriptorSet();
    WritePrefilteredEnvMapGenDescriptorSets();
    WriteBRDFIntegrationLUTGenDescriptorSet();
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
        COMMON_MATERIAL mtl = {};

        const gltf::PBRData& pbrData = material.pbrData;
        
        mtl.ALBEDO_MULT.r = pbrData.baseColorFactor.x();
        mtl.ALBEDO_MULT.g = pbrData.baseColorFactor.y();
        mtl.ALBEDO_MULT.b = pbrData.baseColorFactor.z();
        mtl.ALBEDO_MULT.a = pbrData.baseColorFactor.w();
        mtl.METALNESS_SCALE = pbrData.metallicFactor;
        mtl.ROUGHNESS_SCALE = pbrData.roughnessFactor;

        const auto& albedoTexOpt = pbrData.baseColorTexture;
        if (albedoTexOpt.has_value()) {
            const gltf::Texture& tex = asset.textures[albedoTexOpt.value().textureIndex];
            mtl.ALBEDO_TEX_IDX = tex.imageIndex.has_value() ? tex.imageIndex.value() : -1;
        }

        mtl.NORMAL_SCALE = 1.f;

        const auto& normalTexOpt = material.normalTexture;
        if (normalTexOpt.has_value()) {
            const gltf::Texture& tex = asset.textures[normalTexOpt.value().textureIndex];
            mtl.NORMAL_TEX_IDX = tex.imageIndex.has_value() ? tex.imageIndex.value() : -1;
        
            mtl.NORMAL_SCALE = normalTexOpt.value().scale;
        }

        const auto& mrTexOpt = material.pbrData.metallicRoughnessTexture;
        if (mrTexOpt.has_value()) {
            const gltf::Texture& tex = asset.textures[mrTexOpt.value().textureIndex];
            mtl.MR_TEX_IDX = tex.imageIndex.has_value() ? tex.imageIndex.value() : -1;
        }

        mtl.AO_COEF = 1.f;

        const auto& aoTexOpt = material.occlusionTexture;
        if (aoTexOpt.has_value()) {
            const gltf::Texture& tex = asset.textures[aoTexOpt.value().textureIndex];
            mtl.AO_TEX_IDX = tex.imageIndex.has_value() ? tex.imageIndex.value() : -1;
        
            mtl.AO_COEF = aoTexOpt.value().strength;
        }

        const auto& emissiveTexOpt = material.emissiveTexture;
        if (emissiveTexOpt.has_value()) {
            const gltf::Texture& tex = asset.textures[emissiveTexOpt.value().textureIndex];
            mtl.EMISSIVE_TEX_IDX = tex.imageIndex.has_value() ? tex.imageIndex.value() : -1;
        }

        mtl.EMISSIVE_MULT.r = material.emissiveFactor.x();
        mtl.EMISSIVE_MULT.g = material.emissiveFactor.y();
        mtl.EMISSIVE_MULT.b = material.emissiveFactor.z();

        mtl.FLAGS = 0;
        mtl.FLAGS |= (material.doubleSided ? glm::uint(COMMON_MATERIAL_FLAGS::DOUBLE_SIDED) : glm::uint(0));

        if (material.alphaMode == gltf::AlphaMode::Mask) {
            mtl.FLAGS |= glm::uint(COMMON_MATERIAL_FLAGS::ALPHA_KILL);
        } else if (material.alphaMode == gltf::AlphaMode::Blend) {
            mtl.FLAGS |= glm::uint(COMMON_MATERIAL_FLAGS::ALPHA_BLEND);
        }

        mtl.ALPHA_REF = material.alphaCutoff;

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

                    s_cpuInstData.emplace_back(instInfo);
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
            imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageCreateInfo.mipLevels = texData.GetMipsCount();
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

                vkn::Texture& texture = s_commonMaterialTextures[textureIdx];

                CmdPipelineImageBarrier(
                    cmdBuffer,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_2_NONE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_NONE,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    texture.Get(),
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    0,
                    1
                );

                VkCopyBufferToImageInfo2 copyInfo = {};

                copyInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
                copyInfo.srcBuffer = s_commonStagingBuffers[j].Get();
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

                const TextureLoadData& texData = s_cpuTexturesData[textureIdx];

                GenerateTextureMipmaps(cmdBuffer, texture, texData);

                for (uint32_t i = 0; i < texData.GetMipsCount(); ++i) {
                    CmdPipelineImageBarrier(
                        cmdBuffer,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_2_TRANSFER_READ_BIT,
                        VK_ACCESS_2_SHADER_READ_BIT,
                        texture.Get(),
                        VK_IMAGE_ASPECT_COLOR_BIT,
                        i,
                        1
                    );
                }
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

    const glm::float4x4& viewMatrix = s_camera.GetViewMatrix();
    const glm::float4x4& projMatrix = s_camera.GetProjMatrix();
    const glm::float4x4& viewProjMatrix = s_camera.GetViewProjMatrix();

    pCommonConstBufferData->VIEW_MATRIX = viewMatrix;
    pCommonConstBufferData->PROJ_MATRIX = projMatrix;
    pCommonConstBufferData->VIEW_PROJ_MATRIX = viewProjMatrix;

    pCommonConstBufferData->INV_VIEW_MATRIX = glm::inverse(viewMatrix);
    pCommonConstBufferData->INV_PROJ_MATRIX = glm::inverse(projMatrix);
    pCommonConstBufferData->INV_VIEW_PROJ_MATRIX = glm::inverse(viewProjMatrix);

    memcpy(&pCommonConstBufferData->CAMERA_FRUSTUM, &s_camera.GetFrustum(), sizeof(FRUSTUM));
    
    pCommonConstBufferData->SCREEN_SIZE.x = s_pWnd->GetWidth();
    pCommonConstBufferData->SCREEN_SIZE.y = s_pWnd->GetHeight();

    pCommonConstBufferData->Z_NEAR = s_camera.GetZNear();
    pCommonConstBufferData->Z_FAR = s_camera.GetZFar();

    uint32_t dbgVisFlags = DBG_RT_OUTPUT_MASKS[s_dbgOutputRTIdx];
    uint32_t dbgFlags = 0;

    dbgFlags |= TONEMAPPING_MASKS[s_tonemappingPreset];
    dbgFlags |= s_useMeshIndirectDraw ? (uint32_t)COMMON_DBG_FLAG_MASKS::USE_MESH_INDIRECT_DRAW_MASK : 0;
    dbgFlags |= s_useMeshCulling      ? (uint32_t)COMMON_DBG_FLAG_MASKS::USE_MESH_GPU_CULLING_MASK : 0;
    dbgFlags |= s_useIndirectLighting ? (uint32_t)COMMON_DBG_FLAG_MASKS::USE_INDIRECT_LIGHTING_MASK : 0;

    pCommonConstBufferData->COMMON_DBG_FLAGS = dbgFlags;
    pCommonConstBufferData->COMMON_DBG_VIS_FLAGS = dbgVisFlags;

    pCommonConstBufferData->CAM_WPOS = glm::float4(s_camera.GetPosition(), 0.f);

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


static void PrecomputeIBLIrradianceMap(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Precompute_IBL_Irradiance_Map", 165, 42, 42, 255);
    Timer timer;

    for (uint32_t faceIdx = 0; faceIdx < CUBEMAP_FACE_COUNT; ++faceIdx) {
        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_SHADER_WRITE_BIT,
            s_irradianceMapTexture.Get(),
            VK_IMAGE_ASPECT_COLOR_BIT,
            0,
            VK_REMAINING_MIP_LEVELS,
            faceIdx,
            1
        );
    }

    vkCmdBindPipeline(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE, s_irradianceMapGenPipeline);
    
    VkDescriptorSet descSets[] = { s_commonDescriptorSet, s_irradianceMapGenDescriptorSet };
    vkCmdBindDescriptorSets(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE, s_irradianceMapGenPipelineLayout, 0, _countof(descSets), descSets, 0, nullptr);

    IRRADIANCE_MAP_PUSH_CONSTS pushConsts = {};
    pushConsts.ENV_MAP_FACE_SIZE.x = s_skyboxTexture.GetSizeX();
    pushConsts.ENV_MAP_FACE_SIZE.y = s_skyboxTexture.GetSizeY();

    vkCmdPushConstants(cmdBuffer.Get(), s_irradianceMapGenPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(IRRADIANCE_MAP_PUSH_CONSTS), &pushConsts);

    cmdBuffer.CmdDispatch( 
        ceil(COMMON_IRRADIANCE_MAP_SIZE.x / 32.f),
        ceil(COMMON_IRRADIANCE_MAP_SIZE.y / 32.f), 
        6
    );

    for (uint32_t faceIdx = 0; faceIdx < CUBEMAP_FACE_COUNT; ++faceIdx) {
        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_WRITE_BIT,
            VK_ACCESS_2_SHADER_READ_BIT,
            s_irradianceMapTexture.Get(),
            VK_IMAGE_ASPECT_COLOR_BIT,
            0,
            VK_REMAINING_MIP_LEVELS,
            faceIdx,
            1
        );
    }

    CORE_LOG_INFO("Irradiance map generation finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void PrecomputeIBLPrefilteredEnvMap(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Precompute_IBL_Prefiltered_Env_Map", 165, 42, 42, 255);
    Timer timer;

    vkCmdBindPipeline(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE, s_prefilteredEnvMapGenPipeline);

    PREFILTERED_ENV_MAP_PUSH_CONSTS pushConsts = {};
    pushConsts.ENV_MAP_FACE_SIZE.x = s_skyboxTexture.GetSizeX();
    pushConsts.ENV_MAP_FACE_SIZE.y = s_skyboxTexture.GetSizeY();

    CmdPipelineImageBarrier(
        cmdBuffer,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_2_NONE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_NONE,
        VK_ACCESS_2_SHADER_WRITE_BIT,
        s_prefilteredEnvMapTexture.Get(),
        VK_IMAGE_ASPECT_COLOR_BIT,
        0,
        COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT,
        0,
        CUBEMAP_FACE_COUNT
    );

    for (size_t mip = 0; mip < COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT; ++mip) {
        VkDescriptorSet descSets[] = { s_commonDescriptorSet, s_prefilteredEnvGenDescriptorSets[mip] };
        vkCmdBindDescriptorSets(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE, s_prefilteredEnvMapGenPipelineLayout, 0, _countof(descSets), descSets, 0, nullptr);

        pushConsts.MIP = mip;

        vkCmdPushConstants(cmdBuffer.Get(), s_prefilteredEnvMapGenPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

        const uint32_t sizeX = COMMON_PREFILTERED_ENV_MAP_SIZE.x >> mip;
        const uint32_t sizeY = COMMON_PREFILTERED_ENV_MAP_SIZE.y >> mip;

        cmdBuffer.CmdDispatch((uint32_t)ceil(sizeX / 32.f), (uint32_t)ceil(sizeY / 32.f), 6U);
    }

    CmdPipelineImageBarrier(
        cmdBuffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        s_prefilteredEnvMapTexture.Get(),
        VK_IMAGE_ASPECT_COLOR_BIT,
        0,
        COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT,
        0,
        CUBEMAP_FACE_COUNT
    );

    CORE_LOG_INFO("Prefiltered env map generation finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void PrecomputeIBLBRDFIntergrationLUT(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Precompute_IBL_BRDF_Intergration_LUT", 165, 42, 42, 255);
    Timer timer;

    vkCmdBindPipeline(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE, s_BRDFIntegrationLUTGenPipeline);

    CmdPipelineImageBarrier(
        cmdBuffer,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_2_NONE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_NONE,
        VK_ACCESS_2_SHADER_WRITE_BIT,
        s_brdfLUTTexture.Get(),
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    VkDescriptorSet descSets[] = { s_commonDescriptorSet, s_BRDFIntegrationLUTGenDescriptorSet };
    vkCmdBindDescriptorSets(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE, s_BRDFIntegrationLUTGenPipelineLayout, 0, _countof(descSets), descSets, 0, nullptr);

    cmdBuffer.CmdDispatch((uint32_t)ceil(COMMON_BRDF_INTEGRATION_LUT_SIZE.x / 32.f), (uint32_t)ceil(COMMON_BRDF_INTEGRATION_LUT_SIZE.y / 32.f), 1U);

    CmdPipelineImageBarrier(
        cmdBuffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        s_brdfLUTTexture.Get(),
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    CORE_LOG_INFO("BRDF LUT generation finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


void MeshCullingPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Mesh_Culling_Pass", 50, 50, 200, 255);

    CmdPipelineBufferBarrier(
        cmdBuffer, 
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
        VK_ACCESS_2_MEMORY_READ_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        s_commonOpaqueMeshDrawCmdBuffer.Get()
    );

    CmdPipelineBufferBarrier(
        cmdBuffer, 
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
        VK_ACCESS_2_MEMORY_READ_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        s_commonAKillMeshDrawCmdBuffer.Get()
    );

    CmdPipelineBufferBarrier(
        cmdBuffer, 
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
        VK_ACCESS_2_MEMORY_READ_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        s_commonTranspMeshDrawCmdBuffer.Get()
    );

    CmdPipelineBufferBarrier(
        cmdBuffer, 
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
        VK_ACCESS_2_MEMORY_READ_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        s_commonOpaqueMeshDrawCmdCountBuffer.Get()
    );

    CmdPipelineBufferBarrier(
        cmdBuffer, 
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
        VK_ACCESS_2_MEMORY_READ_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        s_commonAKillMeshDrawCmdCountBuffer.Get()
    );

    CmdPipelineBufferBarrier(
        cmdBuffer, 
        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
        VK_ACCESS_2_MEMORY_READ_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        s_commonTranspMeshDrawCmdCountBuffer.Get()
    );

    CmdPipelineBufferBarrier(
        cmdBuffer, 
        s_useMeshIndirectDraw ? VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT : VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
        VK_ACCESS_2_MEMORY_READ_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        s_commonCulledOpaqueInstInfoIDsBuffer.Get()
    );

    CmdPipelineBufferBarrier(
        cmdBuffer, 
        s_useMeshIndirectDraw ? VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT : VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
        VK_ACCESS_2_MEMORY_READ_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        s_commonCulledAKillInstInfoIDsBuffer.Get()
    );

    CmdPipelineBufferBarrier(
        cmdBuffer, 
        s_useMeshIndirectDraw ? VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT : VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
        VK_ACCESS_2_MEMORY_READ_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        s_commonCulledTranspInstInfoIDsBuffer.Get()
    );

    vkCmdBindPipeline(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE, s_meshCullingPipeline);
    
    VkDescriptorSet descSets[] = { s_commonDescriptorSet, s_meshCullingDescriptorSet };
    vkCmdBindDescriptorSets(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE, s_meshCullingPipelineLayout, 0, _countof(descSets), descSets, 0, nullptr);

    MESH_CULLING_PUSH_CONSTS pushConsts = {};
    pushConsts.INST_COUNT = s_cpuInstData.size();

    vkCmdPushConstants(cmdBuffer.Get(), s_meshCullingPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MESH_CULLING_PUSH_CONSTS), &pushConsts);

    cmdBuffer.CmdDispatch(ceil(s_cpuInstData.size() / 64.f), 1, 1);
}


void RenderPass_Depth(vkn::CmdBuffer& cmdBuffer, bool isAKillPass)
{
    if (!s_useDepthPass) {
        return;
    }

    CmdPipelineImageBarrier(
        cmdBuffer,
        isAKillPass ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        isAKillPass ? VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_2_NONE,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        isAKillPass ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_NONE,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        s_commonDepthRT.Get(),
        VK_IMAGE_ASPECT_DEPTH_BIT
    );

    if (s_useMeshIndirectDraw) {
        CmdPipelineBufferBarrier(
            cmdBuffer, 
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            VK_ACCESS_2_MEMORY_WRITE_BIT,
            VK_ACCESS_2_MEMORY_READ_BIT,
            isAKillPass ? s_commonAKillMeshDrawCmdBuffer.Get() : s_commonOpaqueMeshDrawCmdBuffer.Get()
        );

        CmdPipelineBufferBarrier(
            cmdBuffer, 
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            VK_ACCESS_2_MEMORY_WRITE_BIT,
            VK_ACCESS_2_MEMORY_READ_BIT,
            isAKillPass ? s_commonAKillMeshDrawCmdCountBuffer.Get() : s_commonOpaqueMeshDrawCmdCountBuffer.Get()
        );
    }

    CmdPipelineBufferBarrier(
        cmdBuffer, 
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
        s_useMeshIndirectDraw ? VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT : VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT,
        isAKillPass ? s_commonCulledAKillInstInfoIDsBuffer.Get() : s_commonCulledOpaqueInstInfoIDsBuffer.Get()
    );

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView   = s_commonDepthRTView.Get();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp      = isAKillPass ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
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

        ZPASS_PUSH_CONSTS pushConsts = {};
        pushConsts.IS_AKILL_PASS = isAKillPass;

        if (s_useMeshIndirectDraw) {
            vkCmdPushConstants(cmdBuffer.Get(), s_zpassPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ZPASS_PUSH_CONSTS), &pushConsts);

            if (isAKillPass) {
                cmdBuffer.CmdDrawIndexedIndirect(s_commonAKillMeshDrawCmdBuffer, 0, s_commonAKillMeshDrawCmdCountBuffer, 0, MAX_INDIRECT_DRAW_CMD_COUNT, sizeof(COMMON_INDIRECT_DRAW_CMD));
            } else {
                cmdBuffer.CmdDrawIndexedIndirect(s_commonOpaqueMeshDrawCmdBuffer, 0, s_commonOpaqueMeshDrawCmdCountBuffer, 0, MAX_INDIRECT_DRAW_CMD_COUNT, sizeof(COMMON_INDIRECT_DRAW_CMD));
            }
        } else {
            ENG_PROFILE_SCOPED_MARKER_C("Depth_CPU_Frustum_Culling", 50, 255, 50, 255);

            for (uint32_t i = 0; i < s_cpuInstData.size(); ++i) {
                const COMMON_INST_INFO& instInfo = s_cpuInstData[i];
                const COMMON_MATERIAL& material = s_cpuMaterialData[instInfo.MATERIAL_IDX];

                if (IsTransparentMaterial(material) || (isAKillPass && !IsAKillMaterial(material)) || (!isAKillPass && !IsOpaqueMaterial(material))) {
                    continue;
                }

                if (s_useMeshCulling) {
                    if (!IsInstVisible(instInfo)) {
                        continue;
                    }
                }

                pushConsts.INST_INFO_IDX = i;

                vkCmdPushConstants(cmdBuffer.Get(), s_zpassPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ZPASS_PUSH_CONSTS), &pushConsts);

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
        s_commonDepthRT.Get(),
        VK_IMAGE_ASPECT_DEPTH_BIT
    );
}


void DepthPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Depth_Pass", 128, 128, 128, 255);

    {
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Depth_Pass_Opaque", 128, 128, 128, 255);
        RenderPass_Depth(cmdBuffer, false);
    }
    {
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Depth_Pass_AKill", 128, 128, 128, 255);
        RenderPass_Depth(cmdBuffer, true);
    }
}


void RenderPass_GBuffer(vkn::CmdBuffer& cmdBuffer, bool isAKillPass)
{
    for (vkn::Texture& colorRT : s_gbufferRTs) {
        CmdPipelineImageBarrier(
            cmdBuffer,
            isAKillPass ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            isAKillPass ? VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            isAKillPass ? VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_NONE,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            colorRT.Get(),
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    }

    if (s_useMeshIndirectDraw) {
        CmdPipelineBufferBarrier(
            cmdBuffer, 
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            VK_ACCESS_2_MEMORY_WRITE_BIT,
            VK_ACCESS_2_MEMORY_READ_BIT,
            isAKillPass ? s_commonAKillMeshDrawCmdBuffer.Get() : s_commonOpaqueMeshDrawCmdBuffer.Get()
        );

        CmdPipelineBufferBarrier(
            cmdBuffer, 
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            VK_ACCESS_2_MEMORY_WRITE_BIT,
            VK_ACCESS_2_MEMORY_READ_BIT,
            isAKillPass ? s_commonAKillMeshDrawCmdCountBuffer.Get() : s_commonOpaqueMeshDrawCmdCountBuffer.Get()
        );
    }

    CmdPipelineBufferBarrier(
        cmdBuffer, 
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
        s_useMeshIndirectDraw ? VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT : VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_ACCESS_2_MEMORY_READ_BIT,
        isAKillPass ? s_commonCulledAKillInstInfoIDsBuffer.Get() : s_commonCulledOpaqueInstInfoIDsBuffer.Get()
    );

    if (s_useDepthPass) {
        CmdPipelineImageBarrier(
            cmdBuffer,
            isAKillPass ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            isAKillPass ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT : VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            s_commonDepthRT.Get(),
            VK_IMAGE_ASPECT_DEPTH_BIT
        );
    } else {
        CmdPipelineImageBarrier(
            cmdBuffer,
            isAKillPass ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            isAKillPass ? VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            isAKillPass ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_NONE,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            s_commonDepthRT.Get(),
            VK_IMAGE_ASPECT_DEPTH_BIT
        );
    }
    

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = s_commonDepthRTView.Get();
    depthAttachment.imageLayout = s_useDepthPass ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

#ifdef ENG_BUILD_DEBUG
    if (s_useDepthPass) {
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    } else {
        depthAttachment.loadOp = isAKillPass ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;

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

    std::array<VkRenderingAttachmentInfo, GBUFFER_RT_COUNT> colorAttachments = {};

    for (size_t i = 0; i < colorAttachments.size(); ++i) {
        VkRenderingAttachmentInfo& attachment = colorAttachments[i];

        attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        attachment.imageView = s_gbufferRTViews[i].Get();
        attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.loadOp = isAKillPass ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
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

        GBUFFER_PUSH_CONSTS pushConsts = {};
        pushConsts.IS_AKILL_PASS = isAKillPass;

        if (s_useMeshIndirectDraw) {
            vkCmdPushConstants(cmdBuffer.Get(), s_gbufferRenderPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GBUFFER_PUSH_CONSTS), &pushConsts);

            if (isAKillPass) {
                cmdBuffer.CmdDrawIndexedIndirect(s_commonAKillMeshDrawCmdBuffer, 0, s_commonAKillMeshDrawCmdCountBuffer, 0, MAX_INDIRECT_DRAW_CMD_COUNT, sizeof(COMMON_INDIRECT_DRAW_CMD));
            } else {
                cmdBuffer.CmdDrawIndexedIndirect(s_commonOpaqueMeshDrawCmdBuffer, 0, s_commonOpaqueMeshDrawCmdCountBuffer, 0, MAX_INDIRECT_DRAW_CMD_COUNT, sizeof(COMMON_INDIRECT_DRAW_CMD));
            }
        } else {
            ENG_PROFILE_SCOPED_MARKER_C("GBuffer_CPU_Frustum_Culling", 50, 255, 50, 255);

        #ifdef ENG_BUILD_DEBUG
            if (isAKillPass) {
                s_dbgDrawnAkillMeshCount = 0;
            } else {
                s_dbgDrawnOpaqueMeshCount = 0;
            }
        #endif

            for (uint32_t i = 0; i < s_cpuInstData.size(); ++i) {
                const COMMON_INST_INFO& instInfo = s_cpuInstData[i];
                const COMMON_MATERIAL& material = s_cpuMaterialData[instInfo.MATERIAL_IDX];

                if (IsTransparentMaterial(material) || (isAKillPass && !IsAKillMaterial(material)) || (!isAKillPass && !IsOpaqueMaterial(material))) {
                    continue;
                }

                if (s_useMeshCulling) {
                    if (!IsInstVisible(instInfo)) {
                        continue;
                    }
                }

            #ifdef ENG_BUILD_DEBUG
                if (isAKillPass) {
                    ++s_dbgDrawnAkillMeshCount;
                } else {
                    ++s_dbgDrawnOpaqueMeshCount;
                }
            #endif

                pushConsts.INST_INFO_IDX = i;

                vkCmdPushConstants(cmdBuffer.Get(), s_gbufferRenderPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GBUFFER_PUSH_CONSTS), &pushConsts);

                const COMMON_MESH_INFO& mesh = s_cpuMeshData[s_cpuInstData[i].MESH_IDX];
                cmdBuffer.CmdDrawIndexed(mesh.INDEX_COUNT, 1, mesh.FIRST_INDEX, mesh.FIRST_VERTEX, i);
            }
        }
    cmdBuffer.CmdEndRendering();
}


void GBufferRenderPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "GBuffer_Pass", 50, 200, 50, 255);

    {
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "GBuffer_Pass_Opaque", 50, 200, 50, 255);
        RenderPass_GBuffer(cmdBuffer, false);
    }
    {
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "GBuffer_Pass_AKill", 50, 200, 50, 255);
        RenderPass_GBuffer(cmdBuffer, true);
    }
}


void DeferredLightingPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Deferred_Lighting_Pass", 250, 250, 40, 255);

    for (vkn::Texture& colorRT : s_gbufferRTs) {
        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            colorRT.Get(),
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    }

    CmdPipelineImageBarrier(
        cmdBuffer,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_2_NONE,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_NONE,
        VK_ACCESS_2_SHADER_WRITE_BIT,
        s_colorRT.Get(),
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    CmdPipelineImageBarrier(
        cmdBuffer,
        s_useDepthPass ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        s_commonDepthRT.Get(),
        VK_IMAGE_ASPECT_DEPTH_BIT
    );
    

    vkCmdBindPipeline(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE, s_deferredLightingPipeline);
    
    VkDescriptorSet descSets[] = { s_commonDescriptorSet, s_deferredLightingDescriptorSet };
    vkCmdBindDescriptorSets(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_COMPUTE, s_deferredLightingPipelineLayout, 0, _countof(descSets), descSets, 0, nullptr);

    cmdBuffer.CmdDispatch(ceil(s_pWnd->GetWidth() / 32.f), ceil(s_pWnd->GetHeight() / 32.f), 1);
}


void SkyboxPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Skybox_Pass", 255, 165, 10, 255);

    CmdPipelineImageBarrier(
        cmdBuffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        s_colorRT.Get(),
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    CmdPipelineImageBarrier(
        cmdBuffer,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_SHADER_READ_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        s_commonDepthRT.Get(),
        VK_IMAGE_ASPECT_DEPTH_BIT
    );

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;

    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = s_colorRTView.Get(),
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = s_commonDepthRTView.Get();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
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

        vkCmdBindPipeline(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, s_skyboxPipeline);
        
        VkDescriptorSet descSets[] = { s_commonDescriptorSet, s_skyboxDescriptorSet };
        vkCmdBindDescriptorSets(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, s_skyboxPipelineLayout, 0, _countof(descSets), descSets, 0, nullptr);

        cmdBuffer.CmdDraw(36, 1, 0, 0);        
    cmdBuffer.CmdEndRendering();
}


void PostProcessingPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Post_Processing_Pass", 100, 250, 250, 255);

    CmdPipelineImageBarrier(
        cmdBuffer,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        s_colorRT.Get(),
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    CmdPipelineImageBarrier(
        cmdBuffer,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_NONE,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_NONE,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        s_vkSwapchain.GetImage(s_nextImageIdx),
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;

    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = s_vkSwapchain.GetImageView(s_nextImageIdx);
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color.float32[0] = 0.f;
    colorAttachment.clearValue.color.float32[1] = 0.f;
    colorAttachment.clearValue.color.float32[2] = 0.f;
    colorAttachment.clearValue.color.float32[3] = 0.f;
    
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

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

        vkCmdBindPipeline(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, s_postProcessingPipeline);
        
        VkDescriptorSet descSets[] = { s_commonDescriptorSet, s_postProcessingDescriptorSet };
        vkCmdBindDescriptorSets(cmdBuffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, s_postProcessingPipelineLayout, 0, _countof(descSets), descSets, 0, nullptr);

        cmdBuffer.CmdDraw(6, 1, 0, 0);        
    cmdBuffer.CmdEndRendering();
}


static void DebugUIRenderPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Dbg_UI_Render_Pass", 200, 50, 50, 255);

    DbgUI::FillData();
    DbgUI::EndFrame();

    CmdPipelineImageBarrier(
        cmdBuffer,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        s_vkSwapchain.GetImage(s_nextImageIdx),
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;

    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = s_vkSwapchain.GetImageView(s_nextImageIdx);
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    cmdBuffer.CmdBeginRendering(renderingInfo);
        DbgUI::Render(cmdBuffer);
    cmdBuffer.CmdEndRendering();
}


static void RenderScene()
{
    if (s_renderFinishedFence.GetStatus() == VK_NOT_READY) {
        DbgUI::EndFrame();
        return;
    }

    ENG_PROFILE_SCOPED_MARKER_C("Render_Scene", 255, 255, 50, 255);

    UpdateGPUCommonConstBuffer();

    const VkResult acquireResult = vkAcquireNextImageKHR(s_vkDevice.Get(), s_vkSwapchain.Get(), 10'000'000'000, s_presentFinishedSemaphore.Get(), VK_NULL_HANDLE, &s_nextImageIdx);
    
    if (acquireResult != VK_SUBOPTIMAL_KHR && acquireResult != VK_ERROR_OUT_OF_DATE_KHR) {
        VK_CHECK(acquireResult);
    } else {
        s_swapchainRecreateRequired = true;
        DbgUI::EndFrame();
        return;
    }

    vkn::Semaphore& renderingFinishedSemaphore = s_renderFinishedSemaphores[s_nextImageIdx];
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
        DeferredLightingPass(cmdBuffer);

        SkyboxPass(cmdBuffer);

        PostProcessingPass(cmdBuffer);

        DebugUIRenderPass(cmdBuffer);
        
        CmdPipelineImageBarrier(
            cmdBuffer,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_2_SHADER_WRITE_BIT,
            VK_ACCESS_2_NONE,
            s_vkSwapchain.GetImage(s_nextImageIdx),
            VK_IMAGE_ASPECT_COLOR_BIT
        );

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

    PresentImage(s_nextImageIdx);
}


static bool ResizeVkSwapchain(Window* pWnd)
{
    if (!s_swapchainRecreateRequired) {
        return false;
    }

    bool resizeResult;
    s_vkSwapchain.Resize(pWnd->GetWidth(), pWnd->GetHeight(), resizeResult);
    
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
            const glm::quat newRotation   = glm::normalize(glm::quatLookAt(cameraDir, cameraUp));

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

        ResizeDynamicRenderTargets();

        WriteDeferredLightingDescriptorSet();
        WritePostProcessingDescriptorSet();
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

    CreateDynamicRenderTargets();

    CreateCommonSamplers();
    CreateCommonConstBuffer();
    CreateCullingResources();
    CreateCommonDbgTextures();
    CreateDesriptorSets();
    CreatePipelines();

    std::array skyBoxFaceFilepaths = {
        fs::path("../assets/TestPBR/textures/skybox/1024/px.hdr"),
        fs::path("../assets/TestPBR/textures/skybox/1024/nx.hdr"),
        fs::path("../assets/TestPBR/textures/skybox/1024/py.hdr"),
        fs::path("../assets/TestPBR/textures/skybox/1024/ny.hdr"),
        fs::path("../assets/TestPBR/textures/skybox/1024/pz.hdr"),
        fs::path("../assets/TestPBR/textures/skybox/1024/nz.hdr"),
    };
    CreateSkybox(skyBoxFaceFilepaths);

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
    // LoadScene(argc > 1 ? argv[1] : "../assets/TestPBR/TestPBR.gltf");

    UploadGPUResources();
    CreateIBLResources();

    WriteDescriptorSets();

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        PrecomputeIBLIrradianceMap(cmdBuffer);
        PrecomputeIBLPrefilteredEnvMap(cmdBuffer);
        PrecomputeIBLBRDFIntergrationLUT(cmdBuffer);
    });

    s_cpuTexturesData.clear();

    s_camera.SetPosition(glm::float3(0.f, 0.f, 4.f));
    s_camera.SetRotation(glm::quatLookAt(-M3D_AXIS_Z, M3D_AXIS_Y));
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
    
    vkDestroyPipeline(s_vkDevice.Get(), s_deferredLightingPipeline, nullptr);
    vkDestroyPipelineLayout(s_vkDevice.Get(), s_deferredLightingPipelineLayout, nullptr);
    
    vkDestroyPipeline(s_vkDevice.Get(), s_postProcessingPipeline, nullptr);
    vkDestroyPipelineLayout(s_vkDevice.Get(), s_postProcessingPipelineLayout, nullptr);

    vkDestroyPipeline(s_vkDevice.Get(), s_skyboxPipeline, nullptr);
    vkDestroyPipelineLayout(s_vkDevice.Get(), s_skyboxPipelineLayout, nullptr);
    
    vkDestroyPipeline(s_vkDevice.Get(), s_irradianceMapGenPipeline, nullptr);
    vkDestroyPipelineLayout(s_vkDevice.Get(), s_irradianceMapGenPipelineLayout, nullptr);

    vkDestroyPipeline(s_vkDevice.Get(), s_prefilteredEnvMapGenPipeline, nullptr);
    vkDestroyPipelineLayout(s_vkDevice.Get(), s_prefilteredEnvMapGenPipelineLayout, nullptr);
    
    vkDestroyPipeline(s_vkDevice.Get(), s_BRDFIntegrationLUTGenPipeline, nullptr);
    vkDestroyPipelineLayout(s_vkDevice.Get(), s_BRDFIntegrationLUTGenPipelineLayout, nullptr);

    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_zpassDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_meshCullingDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_gbufferRenderDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_deferredLightingDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_postProcessingDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_skyboxDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_irradianceMapGenDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_prefilteredEnvMapGenDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_BRDFIntegrationLUTGenDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_vkDevice.Get(), s_commonDescriptorSetLayout, nullptr);
    
    vkDestroyDescriptorPool(s_vkDevice.Get(), s_commonDescriptorSetPool, nullptr);

    DbgUI::Terminate();

    s_pWnd->Destroy();

    wndSysTerminate();

    return 0;
}