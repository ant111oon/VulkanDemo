#include "core/platform/window/window.h"

#ifdef ENG_OS_WINDOWS
    #include "core/platform/native/win32/window/win32_window.h"
#else
    #error Unsupported OS type!
#endif


#ifndef ENG_BUILD_RELEASE
    #define ENG_DEBUG_DRAW_ENABLED
#endif


#include "core/platform/file/file.h"
#include "core/utils/timer.h"

#include "core/math/transform.h"

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
#include "render/core/vulkan/vk_pso.h"
#include "render/core/vulkan/vk_shader.h"
#include "render/core/vulkan/vk_descriptor.h"
#include "render/core/vulkan/vk_query.h"

#include "render/core/vulkan/vk_memory.h"

#include "core/engine/camera/camera.h"
#include "core/engine/profiler/cpu_profiler.h"

#include "render/core/vulkan/vk_profiler.h"

#include "render/debug/dbg_ui.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>


namespace gltf = fastgltf;
namespace fs   = std::filesystem;

using IndexType = uint32_t;
using float2 = glm::float2;
using float3 = glm::float3;
using float4 = glm::float4;
using uint   = glm::uint;
using uint2  = glm::uvec2;
using uint3  = glm::uvec3;
using uint4  = glm::uvec4;
using int2   = glm::ivec2;
using int3   = glm::ivec3;
using int4   = glm::ivec4;
using float4x4 = glm::float4x4;
using float3x4 = glm::float3x4;
using float4x3 = glm::float4x3;
using float3x3 = glm::float3x3;


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


static constexpr uint32_t COMMON_VERTEX_DATA_SIZE_UI = 6;

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

    glm::uint data[COMMON_VERTEX_DATA_SIZE_UI] = {};
};


static constexpr uint32_t DESC_SET_PER_FRAME = 0;
static constexpr uint32_t DESC_SET_PER_DRAW = 1;
static constexpr uint32_t DESC_SET_TOTAL_COUNT = 2;


enum class COMMON_MATERIAL_FLAGS : glm::uint
{
    DOUBLE_SIDED = 0x1,
    ALPHA_KILL = 0x2,
    ALPHA_BLEND = 0x4,
};


struct COMMON_MATERIAL
{
    float4 ALBEDO_MULT;

    float3 EMISSIVE_MULT;
    float ALPHA_REF;

    float NORMAL_SCALE;
    float METALNESS_SCALE;
    float ROUGHNESS_SCALE;
    float AO_COEF;

    int ALBEDO_TEX_IDX = -1;
    int NORMAL_TEX_IDX = -1;
    int MR_TEX_IDX = -1;
    int AO_TEX_IDX = -1;

    uint2 PAD0;
    int EMISSIVE_TEX_IDX = -1;
    uint FLAGS;
};


struct COMMON_MESH_DATA
{
    uint FIRST_VERTEX;
    uint VERTEX_COUNT;
    uint FIRST_INDEX;
    uint INDEX_COUNT;

    float3 BOUNDS_MIN_LCS;
    uint   PADD0;
    float3 BOUNDS_MAX_LCS;
    uint   PADD1;
};


struct COMMON_INST_AABB
{
    COMMON_INST_AABB() = default;
    
    COMMON_INST_AABB(const float3& min, const float3& max)
    {
        Pack(min, max);
    }

    void Pack(const float3& min, const float3& max)
    {
        MIN_MAX_PACKED.x = glm::packHalf2x16(float2(min.x, min.y));
        MIN_MAX_PACKED.y = glm::packHalf2x16(float2(min.z, max.x));
        MIN_MAX_PACKED.z = glm::packHalf2x16(float2(max.y, max.z));
    }

    math::AABB GetAABB() const
    {
        const float3 minn = float3(glm::unpackHalf2x16(MIN_MAX_PACKED.x), glm::unpackHalf2x16(MIN_MAX_PACKED.y).x);
        const float3 maxx = float3(glm::unpackHalf2x16(MIN_MAX_PACKED.y).y, glm::unpackHalf2x16(MIN_MAX_PACKED.z));

        return math::AABB(minn, maxx);
    }

    uint3 MIN_MAX_PACKED; // x - MIN.xy, y - MIN.z and MAX.x, z - MAX.yz
    uint PADDING;
};


struct COMMON_INST_DATA
{
    uint TRANSFORM_IDX;
    uint MATERIAL_IDX;
    uint MESH_IDX;
    uint VOLUME_IDX;
};


struct COMMON_CMD_DRAW_INDEXED_INDIRECT
{
    // NOTE: Don't change order of this variables!!!
    uint INDEX_COUNT;
    uint INSTANCE_COUNT;
    uint FIRST_INDEX;
    int  VERTEX_OFFSET;
    uint FIRST_INSTANCE;
};


struct DBG_LINE_DATA
{
    uint COLOR;
};


struct DBG_TRIANGLE_DATA
{
    uint COLOR;
};


struct PLANE
{
    float3 normal;
    float distance;
};


struct FRUSTUM
{
    PLANE planes[6];
};


static_assert(sizeof(FRUSTUM) == sizeof(math::Frustum));


struct COMMON_CB_DATA
{
    FRUSTUM CAMERA_FRUSTUM;

    float4x4 VIEW_MATRIX;
    float4x4 PROJ_MATRIX;
    float4x4 VIEW_PROJ_MATRIX;

    float4x4 INV_VIEW_MATRIX;
    float4x4 INV_PROJ_MATRIX;
    float4x4 INV_VIEW_PROJ_MATRIX;

    FRUSTUM CULLING_CAMERA_FRUSTUM;    // In most cases is the same as CAMERA_FRUSTUM but can differ if culling debug mode is enabled
    float4x4 CULLING_VIEW_PROJ_MATRIX; // In most cases is the same as VIEW_PROJ_MATRIX but can differ if culling debug mode is enabled

    uint2 SCREEN_SIZE;
    float Z_NEAR;
    float Z_FAR;

    float3 CAM_WPOS;
    uint FLAGS;
    
    uint2 PAD0;
    uint DBG_FLAGS;
    uint DBG_VIS_FLAGS;
};


enum class COMMON_DBG_FLAG_MASKS
{
    USE_REINHARD_TONE_MAPPING_MASK = 0x1,
    USE_PARTIAL_UNCHARTED_2_TONE_MAPPING_MASK = 0x2,
    USE_UNCHARTED_2_TONE_MAPPING_MASK = 0x4,
    USE_ACES_TONE_MAPPING_MASK = 0x8,
    USE_INDIRECT_LIGHTING_MASK = 0x10,
    USE_MESH_INDIRECT_DRAW_MASK = 0x20,
    USE_MESH_GPU_FRUSTUM_CULLING_MASK = 0x40,
    USE_MESH_GPU_CONTRIBUTION_CULLING_MASK = 0x80,
    USE_MESH_GPU_HZB_CULLING_MASK = 0x100,
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


struct MESH_CULLING_PER_DRAW_DATA
{
    uint INST_COUNT;
    uint HZB_MIPS_COUNT;
    float VIS_CONTRIBUTION_FALLOFF;
};


struct ZPASS_PER_DRAW_DATA
{
    uint IS_AKILL_PASS;
    uint INST_INFO_IDX;
};


struct GBUFFER_PER_DRAW_DATA
{
    uint IS_AKILL_PASS;
    uint INST_INFO_IDX;
};


struct IRRADIANCE_MAP_PER_DRAW_DATA
{
    uint2 ENV_MAP_FACE_SIZE;
};


struct PREFILTERED_ENV_MAP_PER_DRAW_DATA
{
    uint2 ENV_MAP_FACE_SIZE;
    uint  MIP;
};


struct HZB_GEN_PER_DRAW_DATA
{
    uint2 DST_MIP_RESOLUTION;
    uint  DST_MIP_IDX;
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
static constexpr size_t COMMON_MESH_DATA_BUFFER_DESCRIPTOR_SLOT = 2;
static constexpr size_t COMMON_TRANSFORMS_DESCRIPTOR_SLOT = 3;
static constexpr size_t COMMON_MATERIALS_DESCRIPTOR_SLOT = 4;
static constexpr size_t COMMON_MTL_TEXTURES_DESCRIPTOR_SLOT = 5;
static constexpr size_t COMMON_INST_DATA_BUFFER_DESCRIPTOR_SLOT = 6;
static constexpr size_t COMMON_VERTEX_DATA_DESCRIPTOR_SLOT = 7;
static constexpr size_t COMMON_DBG_TEXTURES_DESCRIPTOR_SLOT = 8;
static constexpr size_t COMMON_AABB_BUFFER_DESCRIPTOR_SLOT = 9;
static constexpr size_t COMMON_DEPTH_DESCRIPTOR_SLOT = 10;
static constexpr size_t COMMON_HZB_DESCRIPTOR_SLOT = 11;

static constexpr size_t MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT = 0;
static constexpr size_t MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT = 1;
static constexpr size_t MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT = 2;
static constexpr size_t MESH_CULL_OPAQUE_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT = 3;
static constexpr size_t MESH_CULL_AKILL_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT = 4;
static constexpr size_t MESH_CULL_TRANSP_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT = 5;
static constexpr size_t MESH_CULL_VIS_FLAGS_UAV_DESCRIPTOR_SLOT = 6;

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

static constexpr size_t BACKBUFFER_INPUT_COLOR_DESCRIPTOR_SLOT = 0;

static constexpr size_t SKYBOX_TEXTURE_DESCRIPTOR_SLOT = 0;

static constexpr size_t IRRADIANCE_MAP_GEN_ENV_MAP_DESCRIPTOR_SLOT = 0;
static constexpr size_t IRRADIANCE_MAP_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT = 1;

static constexpr size_t PREFILTERED_ENV_MAP_GEN_ENV_MAP_DESCRIPTOR_SLOT = 0;
static constexpr size_t PREFILTERED_ENV_MAP_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT = 1;

static constexpr size_t BRDF_INTEGRATION_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT = 0;

static constexpr size_t DBG_DRAW_LINES_VERTEX_BUFFER_DESCRIPTOR_SLOT = 0;
static constexpr size_t DBG_DRAW_LINES_DATA_DESCRIPTOR_SLOT = 1;
static constexpr size_t DBG_DRAW_TRIANGLES_VERTEX_BUFFER_DESCRIPTOR_SLOT = 2;
static constexpr size_t DBG_DRAW_TRIANGLES_DATA_DESCRIPTOR_SLOT = 3;

static constexpr size_t HZB_SRC_MIPS_DESCRIPTOR_SLOT = 0;
static constexpr size_t HZB_DST_MIPS_UAV_DESCRIPTOR_SLOT = 1;


static constexpr uint32_t COMMON_MATERIAL_TEXTURES_COUNT = 128;

static constexpr uint32_t MAX_INDIRECT_DRAW_CMD_COUNT = 1024;
static constexpr uint32_t MAX_DBG_LINE_COUNT = 16384;
static constexpr uint32_t MAX_DBG_TRIANGLE_COUNT = 2048;

static constexpr uint32_t DBG_LINE_VERTEX_COUNT = 2;
static constexpr uint32_t DBG_TRIANGLE_VERTEX_COUNT = 3;

static constexpr uint32_t DBG_LINE_VERTEX_DATA_SIZE_UI = 2;
static constexpr uint32_t DBG_TRIANGLE_VERTEX_DATA_SIZE_UI = 2;

static constexpr uint32_t DBG_LINE_VERTEX_BUFFER_SIZE = MAX_DBG_LINE_COUNT * DBG_LINE_VERTEX_COUNT * DBG_LINE_VERTEX_DATA_SIZE_UI * sizeof(glm::uint);
static constexpr uint32_t DBG_TRIANGLE_VERTEX_BUFFER_SIZE = MAX_DBG_TRIANGLE_COUNT * DBG_TRIANGLE_VERTEX_COUNT * DBG_TRIANGLE_VERTEX_DATA_SIZE_UI * sizeof(glm::uint);

static constexpr size_t GBUFFER_RT_COUNT = 4;
static constexpr size_t CUBEMAP_FACE_COUNT = 6;

static constexpr size_t STAGING_BUFFER_SIZE  = 96 * 1024 * 1024; // 96 MB
static constexpr size_t STAGING_BUFFER_COUNT = 2;

static constexpr glm::uint  COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT = 10;
static constexpr float      COMMON_PREFILTERED_ENV_MAP_MIP_ROUGHNESS_DELTA = 1.f / (COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1);

static constexpr glm::uvec2 COMMON_IRRADIANCE_MAP_SIZE       = glm::uvec2(32);
static constexpr glm::uvec2 COMMON_PREFILTERED_ENV_MAP_SIZE  = glm::uvec2(glm::uint(1) << (COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1));
static constexpr glm::uvec2 COMMON_BRDF_INTEGRATION_LUT_SIZE = glm::uvec2(512);

static constexpr uint32_t HZB_BUILD_CS_GROUP_SIZE = 8;
static constexpr uint32_t GEOM_CULLING_CS_GPOUP_SIZE = 64;
static constexpr uint32_t HZB_MAX_MIP_COUNT = 12;


static constexpr const char* APP_NAME = "Vulkan Demo";

#if defined(ENG_BUILD_DEBUG)
    constexpr const char* APP_BUILD_TYPE_STR = "DEBUG";
#elif defined(ENG_BUILD_PROFILE)
    constexpr const char* APP_BUILD_TYPE_STR = "PROFILE";
#else
    constexpr const char* APP_BUILD_TYPE_STR = "RELEASE";
#endif  

static constexpr bool VSYNC_ENABLED = false;

static constexpr float CAMERA_SPEED = 0.0075f;


enum class PassID
{
    COMMON,
    GEOM_CULLING_OCCLUDERS,
    GEOM_CULLING_OCCLUSION,
    DEPTH,
    GBUFFER,
    DEFERRED_LIGHTING,
    SKYBOX,
    POST_PROCESSING,
    BACKBUFFER,
#ifdef ENG_DEBUG_DRAW_ENABLED
    DBG_DRAW_LINES,
    DBG_DRAW_TRIANGLES,
#endif
    IRRADIANCE_MAP_GEN,
    BRDF_LUT_GEN,
    PREFILT_ENV_MAP_GEN,
    HZB_GEN,
    COUNT,
};


static constexpr uint32_t GEOM_CULLING_PASS_COUNT = (uint32_t)PassID::GEOM_CULLING_OCCLUSION - (uint32_t)PassID::GEOM_CULLING_OCCLUDERS + 1;


static std::unique_ptr<eng::Window> s_pWnd = nullptr;

static vkn::Instance& s_vkInstance = vkn::GetInstance();
static vkn::Surface& s_vkSurface = vkn::GetSurface();

static vkn::PhysicalDevice& s_vkPhysDevice = vkn::GetPhysicalDevice();
static vkn::Device& s_vkDevice = vkn::GetDevice();

static vkn::Allocator& s_vkAllocator = vkn::GetAllocator();

static vkn::Swapchain& s_vkSwapchain = vkn::GetSwapchain();

static vkn::CmdPool s_commonCmdPool;

static vkn::CmdBuffer* s_pImmediateSubmitCmdBuffer;
static vkn::Fence      s_immediateSubmitFinishedFence;

static std::vector<vkn::Semaphore> s_renderFinishedSemaphores;
static vkn::Semaphore  s_presentFinishedSemaphore;
static vkn::Fence      s_renderFinishedFence;
static vkn::CmdBuffer* s_pRenderCmdBuffer;

static std::array<vkn::Buffer, STAGING_BUFFER_COUNT> s_commonStagingBuffers;

static std::array<vkn::DescriptorSetLayout, (size_t)PassID::COUNT> s_descSetLayouts;

static std::array<vkn::PSOLayout, (size_t)PassID::COUNT> s_PSOLayouts;
static std::array<vkn::PSO,       (size_t)PassID::COUNT> s_PSOs;

static vkn::DescriptorBuffer s_descriptorBuffer;

static vkn::Buffer s_vertexBuffer;
static vkn::Buffer s_indexBuffer;

static vkn::Buffer s_commonConstBuffer;

static vkn::Buffer s_commonMeshDataBuffer;
static vkn::Buffer s_commonMaterialDataBuffer;
static vkn::Buffer s_commonTransformDataBuffer;
static vkn::Buffer s_commonAABBDataBuffer;
static vkn::Buffer s_commonInstDataBuffer;

static vkn::Buffer s_commonOpaqueMeshDrawCmdBuffer;
static vkn::Buffer s_commonCulledOpaqueInstInfoIDsBuffer;

static vkn::Buffer s_commonAKillMeshDrawCmdBuffer;
static vkn::Buffer s_commonCulledAKillInstInfoIDsBuffer;

static vkn::Buffer s_commonTranspMeshDrawCmdBuffer;
static vkn::Buffer s_commonCulledTranspInstInfoIDsBuffer;

static vkn::Buffer s_commonGeomVisFlagsBuffer;

static std::vector<vkn::Texture>     s_commonMaterialTextures;
static std::vector<vkn::TextureView> s_commonMaterialTextureViews;
static std::vector<vkn::Sampler>     s_commonSamplers;

static std::vector<Vertex> s_cpuVertexBuffer;
static std::vector<IndexType> s_cpuIndexBuffer;

static std::vector<TextureLoadData> s_cpuTexturesData;

static std::vector<COMMON_MESH_DATA>   s_cpuMeshData;
static std::vector<COMMON_MATERIAL>    s_cpuMaterialData;
static std::vector<glm::float4x4>      s_cpuTransformData;
static std::vector<COMMON_INST_DATA>   s_cpuInstData;
static std::vector<COMMON_INST_AABB>   s_cpuAABBData;


static std::vector<DBG_LINE_DATA>     s_dbgLineDataCPU;
static std::vector<DBG_TRIANGLE_DATA> s_dbgTriangleDataCPU;

static std::vector<glm::uint> s_dbgLineVertexDataCPU;
static std::vector<glm::uint> s_dbgTriangleVertexDataCPU;

static vkn::Buffer s_dbgLineDataGPU;
static vkn::Buffer s_dbgTriangleDataGPU;
static vkn::Buffer s_dbgLineVertexDataGPU;
static vkn::Buffer s_dbgTriangleVertexDataGPU;


static std::array<vkn::Texture, (size_t)COMMON_DBG_TEX_IDX::COUNT>     s_commonDbgTextures;
static std::array<vkn::TextureView, (size_t)COMMON_DBG_TEX_IDX::COUNT> s_commonDbgTextureViews;

static vkn::Texture     s_skyboxTexture;
static vkn::TextureView s_skyboxTextureView;

static vkn::Texture     s_irradianceMapTexture;
static vkn::TextureView s_irradianceMapTextureView;
static vkn::TextureView s_irradianceMapTextureViewRW;

static vkn::Texture     s_prefilteredEnvMapTexture;
static vkn::TextureView s_prefilteredEnvMapTextureView;
static std::array<vkn::TextureView, COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT * CUBEMAP_FACE_COUNT> s_prefilteredEnvMapTextureViewRWs;

static vkn::Texture     s_brdfLUTTexture;
static vkn::TextureView s_brdfLUTTextureView;
static vkn::TextureView s_brdfLUTTextureViewRW;

static std::array<vkn::Texture, GBUFFER_RT_COUNT>     s_gbufferRTs;
static std::array<vkn::TextureView, GBUFFER_RT_COUNT> s_gbufferRTViews;

static vkn::Texture     s_depthRT;
static vkn::TextureView s_depthRTView;

static vkn::Texture     s_colorRT8U;
static vkn::TextureView s_colorRTView8U;

static vkn::Texture     s_colorRT16F;
static vkn::TextureView s_colorRTView16F;

static vkn::Texture                  s_HZB;
static vkn::TextureView              s_HZBView;
static std::vector<vkn::TextureView> s_HZBMipViews;

static vkn::ComputePSOBuilder s_computePSOBuilder;
static vkn::GraphicsPSOBuilder s_graphicsPSOBuilder;
static std::vector<uint8_t> s_shaderCodeBuffer;

static eng::DbgUI s_dbgUI;

static eng::Camera s_camera;
static glm::float3 s_cameraVel = ZEROF3;

static glm::float4x4 s_fixedCamCullViewProjMatr;
static glm::float4x4 s_fixedCamCullInvViewProjMatr;
static math::Frustum s_fixedCamCullFrustum;

static uint32_t s_dbgOutputRTIdx = 0;
static uint32_t s_nextImageIdx = 0;

static size_t s_frameNumber = 0;
static float s_frameTime = M3D_EPS;
static bool s_swapchainRecreateRequired = false;
static bool s_flyCameraMode = false;
static bool s_cullingTestMode = false;

static bool s_skipRender = false;

static float s_commonVisContributionFalloff = 2.f;

#ifdef ENG_DEBUG_UI_ENABLED
    static bool s_useMeshIndirectDraw = true;
    static bool s_useMeshCulling = true;
    static bool s_useMeshFrustumCulling = true;
    static bool s_useMeshContributionCulling = true;
    static bool s_useMeshHZBCulling = true;
    static bool s_useDepthPass = true;
    static bool s_useIndirectLighting = true;
    static bool s_drawInstAABBs = false;

    // Uses for debug purposes during CPU frustum culling
    static size_t s_dbgDrawnOpaqueMeshCount = 0;
    static size_t s_dbgDrawnAkillMeshCount = 0;
    static size_t s_dbgDrawnTranspMeshCount = 0;

    static uint32_t s_tonemappingPreset = _countof(TONEMAPPING_MASKS) - 1;
#else
    static constexpr bool s_useMeshIndirectDraw = true;
    static constexpr bool s_useMeshCulling = true;
    static constexpr bool s_useMeshFrustumCulling = true;
    static constexpr bool s_useMeshContributionCulling = true;
    static constexpr bool s_useMeshHZBCulling = true;
    static constexpr bool s_useDepthPass = true;
    static constexpr bool s_useIndirectLighting = true;
    static constexpr bool s_drawInstAABBs = false;

    static constexpr uint32_t s_tonemappingPreset = _countof(TONEMAPPING_MASKS) - 1;

    static_assert(s_tonemappingPreset < _countof(TONEMAPPING_MASKS));
#endif


static math::AABB GetWorldAABB(const math::AABB& aabbLCS, const glm::float4x4& wMatr)
{
    const glm::float3 aabbMin = aabbLCS.min;
    const glm::float3 aabbMax = aabbLCS.max;

    glm::float3 corners[8] = {
        glm::float3(aabbMin.x, aabbMin.y, aabbMin.z),
        glm::float3(aabbMax.x, aabbMin.y, aabbMin.z),
        glm::float3(aabbMin.x, aabbMax.y, aabbMin.z),
        glm::float3(aabbMax.x, aabbMax.y, aabbMin.z),
        glm::float3(aabbMin.x, aabbMin.y, aabbMax.z),
        glm::float3(aabbMax.x, aabbMin.y, aabbMax.z),
        glm::float3(aabbMin.x, aabbMax.y, aabbMax.z),
        glm::float3(aabbMax.x, aabbMax.y, aabbMax.z)
    };

    glm::float3 newMin = glm::float3(std::numeric_limits<float>::max());
    glm::float3 newMax = glm::float3(std::numeric_limits<float>::lowest());

    for (int i = 0; i < 8; i++) {
        const glm::float3 p = glm::float3(wMatr * glm::float4(corners[i], 1.f));

        newMin = glm::min(newMin, p);
        newMax = glm::max(newMax, p);
    }

    return math::AABB(newMin, newMax);
}


static bool IsInstVisible(const COMMON_INST_DATA& instInfo)
{
    ENG_PROFILE_TRANSIENT_SCOPED_MARKER_C("CPU_Is_Inst_Visible", eng::ProfileColor::Purple1);

    const math::AABB aabb = s_cpuAABBData[instInfo.VOLUME_IDX].GetAABB();

    const math::Frustum& frustum = s_camera.GetFrustum();

    return frustum.IsIntersect(aabb); 
}


static void ClearDebugDrawData()
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    s_dbgLineDataCPU.clear();
    s_dbgLineVertexDataCPU.clear();

    s_dbgTriangleDataCPU.clear();
    s_dbgTriangleVertexDataCPU.clear();
#endif
}


static void RenderDebugLine(const glm::float3& wPos0, const glm::float3& wPos1, const glm::float4& color)
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    if (s_dbgLineDataCPU.size() == s_dbgLineDataCPU.capacity()) {
        CORE_LOG_WARN("Debug lines buffer is full");
        return;
    }

    DBG_LINE_DATA data = {};
    data.COLOR = glm::packUnorm4x8(color);

    s_dbgLineDataCPU.emplace_back(data);

    glm::uint wPos0XY = glm::packHalf2x16(glm::float2(wPos0.x, wPos0.y));
    glm::uint wPos0Z = glm::packHalf2x16(glm::float2(wPos0.z, 0.f));

    s_dbgLineVertexDataCPU.emplace_back(wPos0XY);
    s_dbgLineVertexDataCPU.emplace_back(wPos0Z);

    glm::uint wPos1XY = glm::packHalf2x16(glm::float2(wPos1.x, wPos1.y));
    glm::uint wPos1Z = glm::packHalf2x16(glm::float2(wPos1.z, 0.f));

    s_dbgLineVertexDataCPU.emplace_back(wPos1XY);
    s_dbgLineVertexDataCPU.emplace_back(wPos1Z);
#endif
}


static void RenderDebugTriangleWire(const glm::float3& wPos0, const glm::float3& wPos1, const glm::float3& wPos2, const glm::float4& color)
{
    RenderDebugLine(wPos0, wPos1, color);
    RenderDebugLine(wPos1, wPos2, color);
    RenderDebugLine(wPos2, wPos0, color);
}


static void RenderDebugTriangleFilled(const glm::float3& wPos0, const glm::float3& wPos1, const glm::float3& wPos2, const glm::float4& color)
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    if (s_dbgTriangleDataCPU.size() == s_dbgTriangleDataCPU.capacity()) {
        CORE_LOG_WARN("Debug triangles buffer is full");
        return;
    }

    DBG_TRIANGLE_DATA data = {};
    data.COLOR = glm::packUnorm4x8(color);

    s_dbgTriangleDataCPU.emplace_back(data);

    glm::uint wPos0XY = glm::packHalf2x16(glm::float2(wPos0.x, wPos0.y));
    glm::uint wPos0Z = glm::packHalf2x16(glm::float2(wPos0.z, 0.f));

    s_dbgTriangleVertexDataCPU.emplace_back(wPos0XY);
    s_dbgTriangleVertexDataCPU.emplace_back(wPos0Z);

    glm::uint wPos1XY = glm::packHalf2x16(glm::float2(wPos1.x, wPos1.y));
    glm::uint wPos1Z = glm::packHalf2x16(glm::float2(wPos1.z, 0.f));

    s_dbgTriangleVertexDataCPU.emplace_back(wPos1XY);
    s_dbgTriangleVertexDataCPU.emplace_back(wPos1Z);

    glm::uint wPos2XY = glm::packHalf2x16(glm::float2(wPos2.x, wPos2.y));
    glm::uint wPos2Z = glm::packHalf2x16(glm::float2(wPos2.z, 0.f));

    s_dbgTriangleVertexDataCPU.emplace_back(wPos2XY);
    s_dbgTriangleVertexDataCPU.emplace_back(wPos2Z);
#endif
}


static void RenderDebugQuadWire(const glm::float3& wPos0, const glm::float3& wPos1, const glm::float3& wPos2, const glm::float3& wPos3, const glm::float4& color)
{
    RenderDebugLine(wPos0, wPos1, color);
    RenderDebugLine(wPos1, wPos2, color);
    RenderDebugLine(wPos2, wPos3, color);
    RenderDebugLine(wPos3, wPos0, color);
}


static void RenderDebugQuadFilled(const glm::float3& wPos0, const glm::float3& wPos1, const glm::float3& wPos2, const glm::float3& wPos3, const glm::float4& color)
{
    RenderDebugTriangleFilled(wPos0, wPos1, wPos2, color);
    RenderDebugTriangleFilled(wPos0, wPos2, wPos3, color);
}


template <typename Func>
static void RenderDebugOBBInternal(const Func& func, const glm::float4x4& trs, const glm::float4& color)
{    
#ifdef ENG_DEBUG_DRAW_ENABLED
    glm::float3 centerWPos;
    glm::quat rotation;
    glm::float3 size;

    math::GetTRSComponents(trs, centerWPos, rotation, size);

    const glm::float3 axisX = glm::normalize(rotation * M3D_AXIS_X);
    const glm::float3 axisY = glm::normalize(rotation * M3D_AXIS_Y);
    const glm::float3 axisZ = glm::normalize(rotation * M3D_AXIS_Z);

    CORE_ASSERT(math::IsZero(glm::dot(axisX, axisY)));
    CORE_ASSERT(math::IsZero(glm::dot(axisZ, axisY)));
    CORE_ASSERT(math::IsZero(glm::dot(axisZ, axisX)));

    const glm::float3 halfSize = size * 0.5f;
    const glm::float3 origin = centerWPos - halfSize.x * axisX - halfSize.y * axisY + halfSize.z * axisZ;

    const glm::float3 bln = origin;
    const glm::float3 brn = bln + axisX * size.x;
    const glm::float3 urn = brn + axisY * size.y;
    const glm::float3 uln = bln + axisY * size.y;

    const glm::float3 blf = bln - axisZ * size.z;
    const glm::float3 brf = brn - axisZ * size.z;
    const glm::float3 urf = urn - axisZ * size.z;
    const glm::float3 ulf = uln - axisZ * size.z;

    func(bln, brn, urn, uln, color);
    func(brn, brf, urf, urn, color);
    func(brf, blf, ulf, urf, color);
    func(blf, bln, uln, ulf, color);
    func(uln, urn, urf, ulf, color);
    func(blf, brf, brn, bln, color);
#endif
}


template <typename Func>
static void RenderDebugAABBInternal(const Func& func, const glm::float4x4& ts, const glm::float4& color)
{
    RenderDebugOBBInternal(func, math::MakeTRS(math::GetTranslation(ts), M3D_QUAT_IDENTITY, math::GetScale(ts)), color);
}


static void RenderDebugAABBWired(const glm::float4x4& ts, const glm::float4& color)
{
    RenderDebugAABBInternal(RenderDebugQuadWire, ts, color);
}


static void RenderDebugAABBFilled(const glm::float4x4& ts, const glm::float4& color)
{
    RenderDebugAABBInternal(RenderDebugQuadFilled, ts, color);
}


static void RenderDebugOBBWired(const glm::float4x4& trs, const glm::float4& color)
{
    RenderDebugOBBInternal(RenderDebugQuadWire, trs, color);
}


static void RenderDebugOBBFilled(const glm::float4x4& trs, const glm::float4& color)
{
    RenderDebugOBBInternal(RenderDebugQuadFilled, trs, color);
}


template <typename Func>
static void RenderDebugFrustumInternal(const Func& func, const glm::float4x4& frustumInvViewProj, const glm::float4& color)
{    
#ifdef ENG_DEBUG_DRAW_ENABLED
    #ifdef ENG_GFX_API_VULKAN
        static constexpr float BOTTOM_NDC_Y = 1.f;
        static constexpr float TOP_NDC_Y = -1.f;
    #else
        static constexpr float BOTTOM_NDC_Y = -1.f;
        static constexpr float TOP_NDC_Y = 1.f;
    #endif

    #ifdef ENG_REVERSED_Z
        static constexpr float NEAR_NDC_Z = 1.f;
        static constexpr float FAR_NDC_Z = 0.f;
    #else
        static constexpr float NEAR_NDC_Z = 0.f;
        static constexpr float FAR_NDC_Z = 1.f;
    #endif

    glm::float4 bln = frustumInvViewProj * glm::float4(-1.f, BOTTOM_NDC_Y, NEAR_NDC_Z, 1.f);
    bln /= bln.w;
    
    glm::float4 brn = frustumInvViewProj * glm::float4(1.f, BOTTOM_NDC_Y, NEAR_NDC_Z, 1.f);
    brn /= brn.w;

    glm::float4 urn = frustumInvViewProj * glm::float4(1.f, TOP_NDC_Y, NEAR_NDC_Z, 1.f);
    urn /= urn.w;

    glm::float4 uln = frustumInvViewProj * glm::float4(-1.f, TOP_NDC_Y, NEAR_NDC_Z, 1.f);
    uln /= uln.w;

    glm::float4 blf = frustumInvViewProj * glm::float4(-1.f, BOTTOM_NDC_Y, FAR_NDC_Z, 1.f);
    blf /= blf.w;

    glm::float4 brf = frustumInvViewProj * glm::float4(1.f, BOTTOM_NDC_Y, FAR_NDC_Z, 1.f);
    brf /= brf.w;

    glm::float4 urf = frustumInvViewProj * glm::float4(1.f, TOP_NDC_Y, FAR_NDC_Z, 1.f);
    urf /= urf.w;

    glm::float4 ulf = frustumInvViewProj * glm::float4(-1.f, TOP_NDC_Y, FAR_NDC_Z, 1.f);
    ulf /= ulf.w;

    func(bln, brn, urn, uln, color);
    func(brn, brf, urf, urn, color);
    func(brf, blf, ulf, urf, color);
    func(blf, bln, uln, ulf, color);
    func(uln, urn, urf, ulf, color);
    func(blf, brf, brn, bln, color);
#endif
}


static void RenderDebugFrustumWired(const glm::float4x4& frustumInvViewProj, const glm::float4& color)
{
    RenderDebugFrustumInternal(RenderDebugQuadWire, frustumInvViewProj, color);
}


static void RenderDebugFrustumFilled(const glm::float4x4& frustumInvViewProj, const glm::float4& color)
{
    RenderDebugFrustumInternal(RenderDebugQuadFilled, frustumInvViewProj, color);
}


#ifdef ENG_DEBUG_UI_ENABLED
namespace DbgUI
{
    static void FillData()
    {
        if (ImGui::Begin("Settings")) {
            ImGui::SeparatorText("Common Info");
            ImGui::Text("Build Type: %s", APP_BUILD_TYPE_STR);
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

            ImGui::NewLine();
            ImGui::TextDisabled("Vertex Buffer Size: %.3f MB", s_cpuVertexBuffer.size() * sizeof(Vertex) / 1024.f / 1024.f);
            ImGui::TextDisabled("Index Buffer Size: %.3f MB", s_cpuIndexBuffer.size() * sizeof(IndexType) / 1024.f / 1024.f);
            ImGui::TextDisabled("Debug Lines Data Size: %.3f KB", (s_dbgLineDataGPU.GetMemorySize() + s_dbgLineVertexDataGPU.GetMemorySize()) / 1024.f);
            ImGui::TextDisabled("Debug Triangles Data Size: %.3f KB", (s_dbgTriangleDataGPU.GetMemorySize() + s_dbgTriangleVertexDataGPU.GetMemorySize()) / 1024.f);
            
            static constexpr ImVec4 IMGUI_RED_COLOR(1.f, 0.f, 0.f, 1.f);
            static constexpr ImVec4 IMGUI_GREEN_COLOR(0.f, 1.f, 0.f, 1.f);

            ImGui::NewLine();
            ImGui::SeparatorText("Camera Info");
            ImGui::Text("Fly Camera Mode (F5):");
            ImGui::SameLine(); 
            ImGui::TextColored(s_flyCameraMode ? IMGUI_GREEN_COLOR : IMGUI_RED_COLOR, s_flyCameraMode ? "ON" : "OFF");

            ImGui::Text("Fixed Culling Camera (F6):");
            if (ImGui::IsItemHovered()) {
                if (ImGui::BeginTooltip()) {
                    ImGui::Text("Perform culling from fixed pos frustum view");
                } ImGui::EndTooltip();
            }
            ImGui::SameLine(); 
            ImGui::TextColored(s_cullingTestMode ? IMGUI_GREEN_COLOR : IMGUI_RED_COLOR, s_cullingTestMode ? "ON" : "OFF");

            ImGui::NewLine();
            ImGui::SeparatorText("Culling");
            
            ImGui::Checkbox("Mesh Culling", &s_useMeshCulling);

            if (s_useMeshCulling) {
                ImGui::Checkbox("Mesh Frustum Culling", &s_useMeshFrustumCulling);
                ImGui::Checkbox("Mesh HZB Culling", &s_useMeshHZBCulling);
                ImGui::Checkbox("Mesh Contribution Culling", &s_useMeshContributionCulling);

                if (s_useMeshContributionCulling) {
                    ImGui::SliderFloat("##VisContributionFalloff", &s_commonVisContributionFalloff, 0.f, 100.f, "Vis Contrib Falloff: %.1f");

                    if (ImGui::IsItemHovered()) {
                        if (ImGui::BeginTooltip()) {
                            ImGui::Text("If renderable entity size in pixels in any dimension is less then this value than it will be culled");
                        } ImGui::EndTooltip();
                    }
                }
            }

            ImGui::NewLine();
            ImGui::SeparatorText("Depth Pass");
            ImGui::Checkbox("##DepthPassEnabled", &s_useDepthPass);
            ImGui::SameLine();
            ImGui::TextColored(s_useDepthPass ? IMGUI_GREEN_COLOR : IMGUI_RED_COLOR, "Enabled");
            
            ImGui::NewLine();
            ImGui::SeparatorText("GBuffer Pass");
            ImGui::Checkbox("##UseMeshIndirectDraw", &s_useMeshIndirectDraw);
            ImGui::SameLine(); 
            ImGui::TextColored(s_useMeshIndirectDraw ? IMGUI_GREEN_COLOR : IMGUI_RED_COLOR, "Use Indirect Draw");
            
            if (!s_useMeshIndirectDraw) {
                ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.f, 1.f), "(Drawn Opaque Mesh Count: %zu)", s_dbgDrawnOpaqueMeshCount);
                ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.f, 1.f), "(Drawn AKill Mesh Count: %zu)", s_dbgDrawnAkillMeshCount);
                ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.f, 1.f), "(Drawn Transparent Mesh Count: %zu)", s_dbgDrawnTranspMeshCount);
            }

            ImGui::NewLine();
            ImGui::SeparatorText("Deferred Lighting Pass");
            ImGui::Checkbox("##UseIndirectLighting", &s_useIndirectLighting);
            ImGui::SameLine(); 
            ImGui::TextColored(s_useIndirectLighting ? IMGUI_GREEN_COLOR : IMGUI_RED_COLOR, "Use Indirect Lighting");
            
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

            ImGui::Checkbox("##DrawInstanceAABB", &s_drawInstAABBs);
            ImGui::SameLine();
            ImGui::TextColored(s_drawInstAABBs ? IMGUI_GREEN_COLOR : IMGUI_RED_COLOR, "Draw Instance AABB");

        #ifdef ENG_BUILD_DEBUG
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

            // if (ImGui::Begin("Viewport")) {
            //     ImTextureID ID = s_dbgUI.AddTexture(s_colorRTView16F, s_commonSamplers[(size_t)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE]);
                
            //     ImVec2 size = ImGui::GetWindowSize();
            //     size.x = std::min((float)s_colorRT16F.GetSizeX(), size.x);
            //     size.x = std::min((float)s_colorRT16F.GetSizeY(), size.y);

            //     ImGui::Image(ID, size);
            // } ImGui::End();
        } ImGui::End();
    }
}
#endif


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
static void ImmediateSubmitQueue(vkn::Queue& queue, Func func, Args&&... args)
{   
    s_immediateSubmitFinishedFence.Reset();
    s_pImmediateSubmitCmdBuffer->Reset();

    s_pImmediateSubmitCmdBuffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        func(*s_pImmediateSubmitCmdBuffer, std::forward<Args>(args)...);
    s_pImmediateSubmitCmdBuffer->End();

    queue.Submit(*s_pImmediateSubmitCmdBuffer, &s_immediateSubmitFinishedFence);

    s_immediateSubmitFinishedFence.WaitFor(10'000'000'000);
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

    swapchainCreateInfo.minImageCount    = 3;
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
    physDeviceFeturesReq.descriptorIndexing = true;
    physDeviceFeturesReq.descriptorBindingPartiallyBound = true; // Allow to left dome descriptors unwriten
    physDeviceFeturesReq.runtimeDescriptorArray = true;          // Allows to use unsised arrays in shader
    physDeviceFeturesReq.samplerAnisotropy = true;
    physDeviceFeturesReq.samplerMirrorClampToEdge = true;
    physDeviceFeturesReq.vertexPipelineStoresAndAtomics = true;
    physDeviceFeturesReq.bufferDeviceAddress = true;

#ifndef ENG_BUILD_RETAIL
    physDeviceFeturesReq.bufferDeviceAddressCaptureReplay = VK_TRUE;
#endif

    vkn::PhysicalDevicePropertiesRequirenments physDevicePropsReq = {};
    physDevicePropsReq.deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

    vkn::PhysicalDeviceCreateInfo physDeviceCreateInfo = {};
    physDeviceCreateInfo.pInstance = &s_vkInstance;
    physDeviceCreateInfo.pPropertiesRequirenments = &physDevicePropsReq;
    physDeviceCreateInfo.pFeaturesRequirenments = &physDeviceFeturesReq;

    s_vkPhysDevice.Create(physDeviceCreateInfo);
    CORE_ASSERT(s_vkPhysDevice.IsCreated()); 

    constexpr std::array deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
    };

    VkPhysicalDeviceDescriptorBufferFeaturesEXT descBuffFeatures = {};
    descBuffFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    descBuffFeatures.descriptorBuffer = VK_TRUE;

#ifndef ENG_BUILD_RETAIL
    descBuffFeatures.descriptorBufferCaptureReplay = VK_TRUE;
#endif

    // Is used since ImGui hardcoded blend state count to 1 in it's pipeline, so validation layers complain
    // that ImGui pipeline has one blend state but VkRenderingInfo has more than one color attachments
    // TODO: Disable this feature/ Render ImGui in the end with separate pass with one color attachement
    VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT dynRendUnusedAttachmentsFeature = {};
    dynRendUnusedAttachmentsFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT;
    dynRendUnusedAttachmentsFeature.pNext = &descBuffFeatures;

    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    features13.pNext = &dynRendUnusedAttachmentsFeature;

    VkPhysicalDeviceVulkan12Features features12 = {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = &features13;
    
    features12.samplerMirrorClampToEdge = VK_TRUE;
    
    features12.bufferDeviceAddress = VK_TRUE;
#ifndef ENG_BUILD_RETAIL
    features12.bufferDeviceAddressCaptureReplay = VK_TRUE;
#endif

    features12.drawIndirectCount = VK_TRUE;

    features12.descriptorIndexing = VK_TRUE;
    features12.descriptorBindingPartiallyBound = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;

    VkPhysicalDeviceVulkan11Features features11 = {};
    features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features11.pNext = &features12;
    features11.shaderDrawParameters = VK_TRUE; // Enables slang internal shader variables like "SV_VertexID" etc.

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &features11;
    features2.features.samplerAnisotropy = VK_TRUE;
    features2.features.vertexPipelineStoresAndAtomics = VK_TRUE;
    features2.features.wideLines = VK_TRUE;

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
    stagingBufCreateInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT;
    stagingBufCreateInfo.pAllocInfo = &stagingBufAllocInfo;

    for (size_t i = 0; i < s_commonStagingBuffers.size(); ++i) {
        s_commonStagingBuffers[i].Create(stagingBufCreateInfo).SetDebugName("STAGING_BUFFER_%zu", i);
    }
}


static void CreateGBufferRTs()
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
}


static void CreateColorRTs()
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

    rtCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    rtCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    s_colorRT8U.Create(rtCreateInfo).SetDebugName("COMMON_COLOR_RT_U8");
    s_colorRTView8U.Create(s_colorRT8U, mapping, subresourceRange).SetDebugName("COMMON_COLOR_RT_VIEW_U8");


    rtCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    rtCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    s_colorRT16F.Create(rtCreateInfo).SetDebugName("COMMON_COLOR_RT_16F");
    s_colorRTView16F.Create(s_colorRT16F, mapping, subresourceRange).SetDebugName("COMMON_COLOR_RT_VIEW_16F");


    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        cmdBuffer.BeginBarrierList()
            .AddTextureBarrier(s_colorRT16F, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_ASPECT_COLOR_BIT)
        .Push();
    });
}


static void CreateDepthRT()
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
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    rtCreateInfo.format = VK_FORMAT_D32_SFLOAT;
    rtCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    s_depthRT.Create(rtCreateInfo).SetDebugName("COMMON_DEPTH_RT");
    s_depthRTView.Create(s_depthRT, mapping, subresourceRange).SetDebugName("COMMON_DEPTH_RT_VIEW");
}


static void CreateHZB()
{
    const VkExtent3D extent = {s_pWnd->GetWidth(), s_pWnd->GetHeight(), 1};

    const uint32_t mipsCount = glm::floor(glm::log2((float)glm::max(extent.width, extent.height))) + 1;
    CORE_ASSERT(mipsCount <= HZB_MAX_MIP_COUNT);

    vkn::AllocationInfo rtAllocInfo = {};
    rtAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    rtAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::TextureCreateInfo rtCreateInfo = {};
    rtCreateInfo.pDevice = &s_vkDevice;
    rtCreateInfo.type = VK_IMAGE_TYPE_2D;
    rtCreateInfo.extent = extent;
    rtCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    rtCreateInfo.flags = 0;
    rtCreateInfo.arrayLayers = 1;
    rtCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    rtCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    rtCreateInfo.pAllocInfo = &rtAllocInfo;

    rtCreateInfo.format = VK_FORMAT_R32_SFLOAT;
    rtCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    rtCreateInfo.mipLevels = mipsCount;

    s_HZB.Create(rtCreateInfo).SetDebugName("HZB");

    VkComponentMapping mapping = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    s_HZBView.Create(s_HZB, mapping, subresourceRange).SetDebugName("HZB_VIEW");
    
    s_HZBMipViews.resize(mipsCount);

    subresourceRange.levelCount = 1;
    
    for (uint32_t mip = 0; mip < mipsCount; ++mip) {
        subresourceRange.baseMipLevel = mip;

        s_HZBMipViews[mip].Create(s_HZB, mapping, subresourceRange).SetDebugName("HZB_MIP_%u", mip);
    }
}


static void CreateDynamicRenderTargets()
{
    CreateGBufferRTs();
    CreateColorRTs();
    CreateDepthRT();
    CreateHZB();
}


static void DestroyDynamicRenderTargets()
{
    for (size_t i = 0; i < GBUFFER_RT_COUNT; ++i) {
        s_gbufferRTViews[i].Destroy();
        s_gbufferRTs[i].Destroy();
    }

    s_depthRTView.Destroy();
    s_depthRT.Destroy();

    s_colorRTView16F.Destroy();
    s_colorRT16F.Destroy();

    s_colorRTView8U.Destroy();
    s_colorRT8U.Destroy();

    for (vkn::TextureView& mip : s_HZBMipViews) {
        mip.Destroy();
    }
    s_HZBView.Destroy();
    s_HZB.Destroy();
}


static void ResizeDynamicRenderTargets()
{
    DestroyDynamicRenderTargets();
    CreateDynamicRenderTargets();
}


static void GenerateTextureMipmaps(vkn::CmdBuffer& cmdBuffer, vkn::Texture& texture, const TextureLoadData& loadData, uint32_t layerIdx = 0)
{
    CORE_ASSERT(layerIdx < texture.GetLayerCount());

    int32_t mipWidth  = texture.GetSizeX();
    int32_t mipHeight = texture.GetSizeY();

    for (uint32_t mip = 1; mip < loadData.GetMipsCount(); ++mip) {
        cmdBuffer.BeginBarrierList()
            .AddTextureBarrier(texture, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, 
                VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, 1, layerIdx, 1)
            .AddTextureBarrier(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, 
                VK_IMAGE_ASPECT_COLOR_BIT, mip, 1, layerIdx, 1)
        .Push();

        vkn::BlitInfo blit = {};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = mip - 1;
        blit.srcSubresource.baseArrayLayer = layerIdx;
        blit.srcSubresource.layerCount = 1;
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };

        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = mip;
        blit.dstSubresource.baseArrayLayer = layerIdx;
        blit.dstSubresource.layerCount = 1;
        blit.dstOffsets[1] = { mipWidth  > 1 ? mipWidth  / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };

        cmdBuffer.CmdBlitTexture(texture, texture, blit, VK_FILTER_LINEAR);

        if (mipWidth > 1) {
            mipWidth /= 2;
        }

        if (mipHeight > 1) {
            mipHeight /= 2;
        }
    }

    // Add this barrier to get all mips in same VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL layout
    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(texture, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, 
                VK_IMAGE_ASPECT_COLOR_BIT, loadData.GetMipsCount() - 1, 1, layerIdx, 1)
        .Push();
}


static void CreateSkybox(std::span<fs::path> faceDataPaths)
{
    eng::Timer timer;

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

            void* pData = stagingBuffer.Map();
            memcpy(pData, loadData.GetData(), loadData.GetMemorySize());
            stagingBuffer.Unmap();
        }

        ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
            cmdBuffer
                .BeginBarrierList()
                    .AddTextureBarrier(s_skyboxTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT, 
                        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1)
                .Push();

            for (size_t j = 0; j < s_commonStagingBuffers.size(); ++j) {
                const uint32_t faceIdx = i + j;
                
                if (faceIdx >= CUBEMAP_FACE_COUNT) {
                    break;
                }

                vkn::Buffer& stagingBuffer = s_commonStagingBuffers[j];

                vkn::BufferToTextureCopyInfo copyInfo = {};
                copyInfo.texSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyInfo.texSubresource.mipLevel = 0;
                copyInfo.texSubresource.baseArrayLayer = faceIdx;
                copyInfo.texSubresource.layerCount = 1;
                copyInfo.texExtent = s_skyboxTexture.GetSize();

                cmdBuffer.CmdCopyBuffer(stagingBuffer, s_skyboxTexture, copyInfo);
            }
        });
    }

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        for (uint32_t layerIdx = 0; layerIdx < s_skyboxTexture.GetLayerCount(); ++layerIdx) {
            GenerateTextureMipmaps(cmdBuffer, s_skyboxTexture, faceLoadDatas[layerIdx], layerIdx);
        }

        cmdBuffer
            .BeginBarrierList()
                .AddTextureBarrier(s_skyboxTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                    VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
            .Push();
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
        viewCreateInfo.subresourceRange.levelCount = COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = CUBEMAP_FACE_COUNT;
        
        s_prefilteredEnvMapTextureView.Create(viewCreateInfo).SetDebugName("COMMON_PREFILTERED_ENV_MAP_VIEW");
    }
    
    {
        vkn::TextureViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.pOwner = &s_prefilteredEnvMapTexture;
        viewCreateInfo.type = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = s_skyboxTexture.GetFormat();
        viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        for (size_t layer = 0; layer < CUBEMAP_FACE_COUNT; ++layer) {
            viewCreateInfo.subresourceRange.baseArrayLayer = layer;
            viewCreateInfo.subresourceRange.layerCount = 1;

            for (size_t mip = 0; mip < COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT; ++mip) {
                viewCreateInfo.subresourceRange.baseMipLevel = mip;
                viewCreateInfo.subresourceRange.levelCount = 1;
    
                s_prefilteredEnvMapTextureViewRWs[layer * COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT + mip].Create(viewCreateInfo)
                    .SetDebugName("COMMON_PREFILTERED_ENV_MAP_VIEW_RW_LAYER_%zu_MIP_%zu", layer, mip);
            }
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


static void CreateDbgDrawResources()
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    {
        s_dbgLineDataCPU.reserve(MAX_DBG_LINE_COUNT);
    
        vkn::AllocationInfo allocInfo = {};
        allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    
        vkn::BufferCreateInfo createInfo = {};
        createInfo.pDevice = &s_vkDevice;
        createInfo.size = MAX_DBG_LINE_COUNT * sizeof(DBG_LINE_DATA);
        createInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
        createInfo.pAllocInfo = &allocInfo;
    
        s_dbgLineDataGPU.Create(createInfo).SetDebugName("DBG_DRAW_LINE_DATA_BUFFER");
    }

    {
        s_dbgLineVertexDataCPU.reserve(DBG_LINE_VERTEX_BUFFER_SIZE);

        vkn::AllocationInfo allocInfo = {};
        allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    
        vkn::BufferCreateInfo createInfo = {};
        createInfo.pDevice = &s_vkDevice;
        createInfo.size = DBG_LINE_VERTEX_BUFFER_SIZE;
        createInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
        createInfo.pAllocInfo = &allocInfo;
    
        s_dbgLineVertexDataGPU.Create(createInfo).SetDebugName("DBG_DRAW_LINE_VERT_BUFFER");
    }

    {
        s_dbgTriangleDataCPU.reserve(MAX_DBG_TRIANGLE_COUNT);
    
        vkn::AllocationInfo allocInfo = {};
        allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    
        vkn::BufferCreateInfo createInfo = {};
        createInfo.pDevice = &s_vkDevice;
        createInfo.size = MAX_DBG_TRIANGLE_COUNT * sizeof(DBG_TRIANGLE_DATA);
        createInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
        createInfo.pAllocInfo = &allocInfo;
    
        s_dbgTriangleDataGPU.Create(createInfo).SetDebugName("DBG_DRAW_TRIANGLE_DATA_BUFFER");
    }

    {
        s_dbgTriangleVertexDataCPU.reserve(DBG_TRIANGLE_VERTEX_BUFFER_SIZE);

        vkn::AllocationInfo allocInfo = {};
        allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    
        vkn::BufferCreateInfo createInfo = {};
        createInfo.pDevice = &s_vkDevice;
        createInfo.size = DBG_TRIANGLE_VERTEX_BUFFER_SIZE;
        createInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
        createInfo.pAllocInfo = &allocInfo;
    
        s_dbgTriangleVertexDataGPU.Create(createInfo).SetDebugName("DBG_DRAW_TRIANGLE_VERT_BUFFER");
    }
#endif
}


static bool LoadShaderSpirVCode(const fs::path& path, std::vector<uint8_t>& buffer)
{
    const fs::path fullPath = fs::absolute(path);

    if (!eng::ReadFile(buffer, fullPath)) {
        return false;
    }

    VK_ASSERT_MSG(buffer.size() % sizeof(uint32_t) == 0, "Size of SPIR-V byte code of %s must be multiple of %zu", fullPath.string().c_str(), sizeof(uint32_t));

    return true;
}


static void CreateCommonDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(COMMON_SAMPLERS_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLER, (uint32_t)COMMON_SAMPLER_IDX::COUNT, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_CONST_BUFFER_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_MESH_DATA_BUFFER_DESCRIPTOR_SLOT,   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_TRANSFORMS_DESCRIPTOR_SLOT,   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_MATERIALS_DESCRIPTOR_SLOT,    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_MTL_TEXTURES_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, COMMON_MATERIAL_TEXTURES_COUNT, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(COMMON_INST_DATA_BUFFER_DESCRIPTOR_SLOT,   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_VERTEX_DATA_DESCRIPTOR_SLOT,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT),
        vkn::DescriptorInfo::Create(COMMON_DBG_TEXTURES_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, (uint32_t)COMMON_DBG_TEX_IDX::COUNT, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_AABB_BUFFER_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_DEPTH_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_HZB_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_ALL),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[(size_t)PassID::COMMON].Create(createInfo).SetDebugName("COMMON_DESCRIPTOR_SET_LAYOUT");
}


static void CreateZPassDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(ZPASS_OPAQUE_INST_INFO_IDS_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(ZPASS_AKILL_INST_INFO_IDS_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[(size_t)PassID::DEPTH].Create(createInfo).SetDebugName("ZPASS_DESCRIPTOR_SET_LAYOUT");
}


static void CreateGeomCullingOccludersDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(MESH_CULL_OPAQUE_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(MESH_CULL_AKILL_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(MESH_CULL_TRANSP_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(MESH_CULL_VIS_FLAGS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[(size_t)PassID::GEOM_CULLING_OCCLUDERS].Create(createInfo).SetDebugName("GEOM_CULLING_OCCLUDERS_DESCRIPTOR_SET_LAYOUT");
}


static void CreateGeomCullingOcclusionDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(MESH_CULL_OPAQUE_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(MESH_CULL_AKILL_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(MESH_CULL_TRANSP_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(MESH_CULL_VIS_FLAGS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[(size_t)PassID::GEOM_CULLING_OCCLUSION].Create(createInfo).SetDebugName("GEOM_CULLING_OCCLUSION_DESCRIPTOR_SET_LAYOUT");
}


static void CreateGBufferDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(GBUFFER_OPAQUE_INST_INFO_IDS_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(GBUFFER_AKILL_INST_INFO_IDS_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[(size_t)PassID::GBUFFER].Create(createInfo).SetDebugName("GBUFFER_DESCRIPTOR_SET_LAYOUT");
}


static void CreateDeferredLightingDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_OUTPUT_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_GBUFFER_0_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_GBUFFER_1_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_GBUFFER_2_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_GBUFFER_3_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_DEPTH_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_IRRADIANCE_MAP_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_PREFILTERED_ENV_MAP_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_BRDF_LUT_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[(size_t)PassID::DEFERRED_LIGHTING].Create(createInfo).SetDebugName("DEFERRED_LIGHTING_DESCRIPTOR_SET_LAYOUT");
}


static void CreatePostProcessingDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(POST_PROCESSING_INPUT_COLOR_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[(size_t)PassID::POST_PROCESSING].Create(createInfo).SetDebugName("POST_PROCESSING_DESCRIPTOR_SET_LAYOUT");
}


static void CreateBackbufferPassDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(BACKBUFFER_INPUT_COLOR_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[(size_t)PassID::BACKBUFFER].Create(createInfo).SetDebugName("BACK_BUFFER_DESCRIPTOR_SET_LAYOUT");
}


static void CreateSkyboxDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(SKYBOX_TEXTURE_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[(size_t)PassID::SKYBOX].Create(createInfo).SetDebugName("SKYBOX_DESCRIPTOR_SET_LAYOUT");
}


static void CreateIrradianceMapGenDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(IRRADIANCE_MAP_GEN_ENV_MAP_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(IRRADIANCE_MAP_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[(size_t)PassID::IRRADIANCE_MAP_GEN].Create(createInfo).SetDebugName("IRRADIANCE_MAP_GEN_DESCRIPTOR_SET_LAYOUT");
}


static void CreatePrefilteredEnvMapGenDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(PREFILTERED_ENV_MAP_GEN_ENV_MAP_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 
            1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(PREFILTERED_ENV_MAP_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT * CUBEMAP_FACE_COUNT, VK_SHADER_STAGE_COMPUTE_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[(size_t)PassID::PREFILT_ENV_MAP_GEN].Create(createInfo).SetDebugName("PREFILT_ENV_MAP_GEN_DESCRIPTOR_SET_LAYOUT");
}


static void CreateBRDFIntegrationLUTGenDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(BRDF_INTEGRATION_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[(size_t)PassID::BRDF_LUT_GEN].Create(createInfo).SetDebugName("BRDF_INTEGRATION_LUT_GEN_DESCRIPTOR_SET_LAYOUT");
}


static void CreateDbgDrawLineDescriptorSetLayout()
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(DBG_DRAW_LINES_VERTEX_BUFFER_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DBG_DRAW_LINES_DATA_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[(size_t)PassID::DBG_DRAW_LINES].Create(createInfo).SetDebugName("DBG_DRAW_LINES_DESCRIPTOR_SET_LAYOUT");
#endif
}


static void CreateDbgDrawTriangleDescriptorSetLayout()
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(DBG_DRAW_TRIANGLES_VERTEX_BUFFER_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DBG_DRAW_TRIANGLES_DATA_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[(size_t)PassID::DBG_DRAW_TRIANGLES].Create(createInfo).SetDebugName("DBG_DRAW_TRIANGLES_DESCRIPTOR_SET_LAYOUT");
#endif
}


static void CreateHZBGenDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(HZB_SRC_MIPS_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, HZB_MAX_MIP_COUNT, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(HZB_DST_MIPS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, HZB_MAX_MIP_COUNT, VK_SHADER_STAGE_COMPUTE_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[(size_t)PassID::HZB_GEN].Create(createInfo).SetDebugName("HZB_GEN_DESCRIPTOR_SET_LAYOUT");
}


static void CreateDescriptorBuffer()
{
    std::array<vkn::DescriptorSetLayout*, (size_t)PassID::COUNT> layouts = {};
    
    for (size_t i = 0; i < layouts.size(); ++i) {
        CORE_ASSERT_MSG(s_descSetLayouts[i].IsCreated(), "Descriptor Set Layout %u is not created", (uint32_t)i);
        layouts[i] = &s_descSetLayouts[i];
    }

    s_descriptorBuffer.Create(&s_vkDevice, layouts).SetDebugName("COMMON_DESCRIPTOR_BUFFER");
}


static void CreateDescriptorSets()
{
    CreateCommonDescriptorSetLayout();
    CreateZPassDescriptorSetLayout();
    CreateGeomCullingOccludersDescriptorSetLayout();
    CreateGeomCullingOcclusionDescriptorSetLayout();
    CreateGBufferDescriptorSetLayout();
    CreateDeferredLightingDescriptorSetLayout();
    CreatePostProcessingDescriptorSetLayout();
    CreateBackbufferPassDescriptorSetLayout();
    CreateSkyboxDescriptorSetLayout();
    CreateIrradianceMapGenDescriptorSetLayout();
    CreatePrefilteredEnvMapGenDescriptorSetLayout();
    CreateBRDFIntegrationLUTGenDescriptorSetLayout();
    CreateDbgDrawLineDescriptorSetLayout();
    CreateDbgDrawTriangleDescriptorSetLayout();
    CreateHZBGenDescriptorSetLayout();

    CreateDescriptorBuffer();
}


static void CreateGeomCullingOccludersPipelineLayout()
{
    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MESH_CULLING_PER_DRAW_DATA) };

    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[(size_t)PassID::COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[(size_t)PassID::GEOM_CULLING_OCCLUDERS];

    s_PSOLayouts[(size_t)PassID::GEOM_CULLING_OCCLUDERS].Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1))
        .SetDebugName("GEOM_CULLING_OCCLUDERS_PIPELINE_LAYOUT");
}


static void CreateGeomCullingOcclusionPipelineLayout()
{
    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MESH_CULLING_PER_DRAW_DATA) };

    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[(size_t)PassID::COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[(size_t)PassID::GEOM_CULLING_OCCLUSION];

    s_PSOLayouts[(size_t)PassID::GEOM_CULLING_OCCLUSION].Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1))
        .SetDebugName("GEOM_CULLING_OCCLUSION_PIPELINE_LAYOUT");
}


static void CreateZPassPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[(size_t)PassID::COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[(size_t)PassID::DEPTH];

    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ZPASS_PER_DRAW_DATA) };

    s_PSOLayouts[(size_t)PassID::DEPTH].Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1))
        .SetDebugName("ZPASS_PIPELINE_LAYOUT");
}


static void CreateGBufferPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[(size_t)PassID::COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[(size_t)PassID::GBUFFER];

    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GBUFFER_PER_DRAW_DATA) };

    s_PSOLayouts[(size_t)PassID::GBUFFER].Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1)).SetDebugName("GBUFFER_PIPELINE_LAYOUT");
}


static void CreateDeferredLightingPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[(size_t)PassID::COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[(size_t)PassID::DEFERRED_LIGHTING];

    s_PSOLayouts[(size_t)PassID::DEFERRED_LIGHTING].Create(&s_vkDevice, layoutPtrs).SetDebugName("DEFERRED_LIGHTING_PIPELINE_LAYOUT");
}


static void CreatePostProcessingPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[(size_t)PassID::COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[(size_t)PassID::POST_PROCESSING];

    s_PSOLayouts[(size_t)PassID::POST_PROCESSING].Create(&s_vkDevice, layoutPtrs).SetDebugName("POST_PROCESSING_PIPELINE_LAYOUT");
}


static void CreateBackbufferPassPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[(size_t)PassID::COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[(size_t)PassID::BACKBUFFER];

    s_PSOLayouts[(size_t)PassID::BACKBUFFER].Create(&s_vkDevice, layoutPtrs).SetDebugName("BACKBUFFER_PIPELINE_LAYOUT");
}


static void CreateSkyboxPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[(size_t)PassID::COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[(size_t)PassID::SKYBOX];

    s_PSOLayouts[(size_t)PassID::SKYBOX].Create(&s_vkDevice, layoutPtrs).SetDebugName("SKYBOX_PIPELINE_LAYOUT");
}


static void CreateIrradianceMapGenPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[(size_t)PassID::COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[(size_t)PassID::IRRADIANCE_MAP_GEN];

    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(IRRADIANCE_MAP_PER_DRAW_DATA) };

    s_PSOLayouts[(size_t)PassID::IRRADIANCE_MAP_GEN].Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1))
        .SetDebugName("IRRAD_MAP_GEN_PIPELINE_LAYOUT");
}


static void CreatePrefilteredEnvMapGenPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[(size_t)PassID::COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[(size_t)PassID::PREFILT_ENV_MAP_GEN];

    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PREFILTERED_ENV_MAP_PER_DRAW_DATA) };

    s_PSOLayouts[(size_t)PassID::PREFILT_ENV_MAP_GEN].Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1))
        .SetDebugName("PREFILT_ENV_MAP_GET_PIPELINE_LAYOUT");
}


static void CreateBRDFIntegrationLUTGenPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[(size_t)PassID::COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[(size_t)PassID::BRDF_LUT_GEN];

    s_PSOLayouts[(size_t)PassID::BRDF_LUT_GEN].Create(&s_vkDevice, layoutPtrs).SetDebugName("GRDF_LUT_GEN_PIPELINE_LAYOUT");
}


static void CreateDbgDrawLinePipelineLayout()
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[(size_t)PassID::COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[(size_t)PassID::DBG_DRAW_LINES];

    s_PSOLayouts[(size_t)PassID::DBG_DRAW_LINES].Create(&s_vkDevice, layoutPtrs).SetDebugName("DBG_DRAW_LINE_PIPELINE_LAYOUT");
#endif
}


static void CreateDbgDrawTrianglePipelineLayout()
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[(size_t)PassID::COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[(size_t)PassID::DBG_DRAW_TRIANGLES];

    s_PSOLayouts[(size_t)PassID::DBG_DRAW_TRIANGLES].Create(&s_vkDevice, layoutPtrs).SetDebugName("DBG_DRAW_TRIANGLES_PIPELINE_LAYOUT");
#endif
}


static void CreateHZBGenPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[(size_t)PassID::COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[(size_t)PassID::HZB_GEN];

    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(HZB_GEN_PER_DRAW_DATA) };

    s_PSOLayouts[(size_t)PassID::HZB_GEN].Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1)).SetDebugName("HZB_GEN_PIPELINE_LAYOUT");
}


static void CreateGeomCullingOccludersPipeline(const fs::path& csPath)
{
    if (!LoadShaderSpirVCode(csPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", csPath.string().c_str());
    }
    
    vkn::Shader shader;
    shader.Create(&s_vkDevice, VK_SHADER_STAGE_COMPUTE_BIT, s_shaderCodeBuffer).SetDebugName("GEOM_CULLING_OCCLUDERS_COMPUTE_SHADER");

    vkn::PSO& pso = s_PSOs[(size_t)PassID::GEOM_CULLING_OCCLUDERS];

    pso = s_computePSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .SetShader(shader)
        .SetLayout(s_PSOLayouts[(size_t)PassID::GEOM_CULLING_OCCLUDERS])
        .Build();

    pso.SetDebugName("GEOM_CULLING_OCCLUDERS_PSO");
}


static void CreateGeomCullingOcclusionPipeline(const fs::path& csPath)
{
    if (!LoadShaderSpirVCode(csPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", csPath.string().c_str());
    }
    
    vkn::Shader shader;
    shader.Create(&s_vkDevice, VK_SHADER_STAGE_COMPUTE_BIT, s_shaderCodeBuffer).SetDebugName("GEOM_CULLING_OCCLUSION_COMPUTE_SHADER");

    vkn::PSO& pso = s_PSOs[(size_t)PassID::GEOM_CULLING_OCCLUSION];

    pso = s_computePSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .SetShader(shader)
        .SetLayout(s_PSOLayouts[(size_t)PassID::GEOM_CULLING_OCCLUSION])
        .Build();

    pso.SetDebugName("GEOM_CULLING_OCCLUSION_PSO");
}


static void CreateZPassPipeline(const fs::path& vsPath, const fs::path& psPath)
{
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer).SetDebugName("ZPASS_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer).SetDebugName("ZPASS_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[(size_t)PassID::DEPTH];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[(size_t)PassID::DEPTH])
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(1.f)
    #ifdef ENG_REVERSED_Z
        .SetDepthTestState(VK_TRUE, VK_COMPARE_OP_GREATER_OR_EQUAL)
    #else
        .SetDepthTestState(VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
    #endif
        .SetDepthWriteState(VK_TRUE)
        .SetDepthBoundsTestState(VK_TRUE, 0.f, 1.f)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })        
        .SetDepthAttachment(s_depthRT.GetFormat());
    
    pso = s_graphicsPSOBuilder.Build();
    
    pso.SetDebugName("ZPASS_PSO");
}
    

static void CreateGBufferRenderPipeline(const fs::path& vsPath, const fs::path& psPath)
{
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer).SetDebugName("GBUFFER_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer).SetDebugName("GBUFFER_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[(size_t)PassID::GBUFFER];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[(size_t)PassID::GBUFFER])
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(1.f)
        .SetDepthTestState(VK_TRUE, VK_COMPARE_OP_EQUAL)
        .SetDepthWriteState(VK_FALSE)
        .SetDepthBoundsTestState(VK_TRUE, 0.f, 1.f)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR });

    #ifdef ENG_BUILD_DEBUG
        s_graphicsPSOBuilder.AddDynamicState(std::array{ VK_DYNAMIC_STATE_DEPTH_COMPARE_OP, VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE });
    #endif

    for (const vkn::Texture& colorRT : s_gbufferRTs) {
        s_graphicsPSOBuilder.AddColorAttachment(colorRT.GetFormat()); 
    }
    s_graphicsPSOBuilder.SetDepthAttachment(s_depthRT.GetFormat());
    
    pso = s_graphicsPSOBuilder.Build();
    
    pso.SetDebugName("GBUFFER_PSO");
}


static void CreateDeferredLightingPipeline(const fs::path& vsPath, const fs::path& psPath)
{
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer).SetDebugName("DEFERRED_LIGHTING_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer).SetDebugName("DEFERRED_LIGHTING_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[(size_t)PassID::DEFERRED_LIGHTING];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[(size_t)PassID::DEFERRED_LIGHTING])
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(1.f)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .AddColorAttachment(s_colorRT16F.GetFormat());
    
    pso = s_graphicsPSOBuilder.Build();
    
    pso.SetDebugName("DEFERRED_LIGHTING_PSO");
}


static void CreatePostProcessingPipeline(const fs::path& vsPath, const fs::path& psPath)
{
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer).SetDebugName("POST_PROCESSING_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer).SetDebugName("POST_PROCESSING_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[(size_t)PassID::POST_PROCESSING];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[(size_t)PassID::POST_PROCESSING])
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(1.f)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .AddColorAttachment(s_colorRT8U.GetFormat());
    
    pso = s_graphicsPSOBuilder.Build();
    
    pso.SetDebugName("POST_PROCESSING_PSO");
}


static void CreateBackbufferPassPipeline(const fs::path& vsPath, const fs::path& psPath)
{
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer).SetDebugName("BACKBUFFER_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer).SetDebugName("BACKBUFFER_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[(size_t)PassID::BACKBUFFER];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[(size_t)PassID::BACKBUFFER])
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(1.f)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .AddColorAttachment(s_vkSwapchain.GetTextureFormat());
    
    pso = s_graphicsPSOBuilder.Build();
    
    pso.SetDebugName("BACKBUFFER_PSO");
}


static void CreateSkyboxPipeline(const fs::path& vsPath, const fs::path& psPath)
{
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer).SetDebugName("SKYBOX_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer).SetDebugName("SKYBOX_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[(size_t)PassID::SKYBOX];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[(size_t)PassID::SKYBOX])
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_NONE)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(1.f)
    #ifdef ENG_REVERSED_Z
        .SetDepthTestState(VK_TRUE, VK_COMPARE_OP_GREATER_OR_EQUAL)
    #else
        .SetDepthTestState(VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
    #endif
        .SetDepthWriteState(VK_FALSE)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .AddColorAttachment(s_colorRT16F.GetFormat())
        .SetDepthAttachment(s_depthRT.GetFormat());
    
    pso = s_graphicsPSOBuilder.Build();
    
    pso.SetDebugName("SKYBOX_PSO");
}


static void CreateIrradianceMapGenPipeline(const fs::path& csPath)
{
    if (!LoadShaderSpirVCode(csPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", csPath.string().c_str());
    }
    
    vkn::Shader shader;
    shader.Create(&s_vkDevice, VK_SHADER_STAGE_COMPUTE_BIT, s_shaderCodeBuffer).SetDebugName("IRRADIANCE_MAP_GEN_COMPUTE_SHADER");

    vkn::PSO& pso = s_PSOs[(size_t)PassID::IRRADIANCE_MAP_GEN];

    pso = s_computePSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .SetShader(shader)
        .SetLayout(s_PSOLayouts[(size_t)PassID::IRRADIANCE_MAP_GEN])
        .Build();

    pso.SetDebugName("IRRADIANCE_MAP_GEN_PSO");
}


static void CreatePrefilteredEnvMapGenPipeline(const fs::path& csPath)
{
    if (!LoadShaderSpirVCode(csPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", csPath.string().c_str());
    }
    
    vkn::Shader shader;
    shader.Create(&s_vkDevice, VK_SHADER_STAGE_COMPUTE_BIT, s_shaderCodeBuffer).SetDebugName("PREFILT_ENV_MAP_GEN_COMPUTE_SHADER");

    vkn::PSO& pso = s_PSOs[(size_t)PassID::PREFILT_ENV_MAP_GEN];

    pso = s_computePSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .SetShader(shader)
        .SetLayout(s_PSOLayouts[(size_t)PassID::PREFILT_ENV_MAP_GEN])
        .Build();

    pso.SetDebugName("PREFILT_ENV_MAP_GEN_PSO");
}


static void CreateBRDFIntegrationLUTGenPipeline(const fs::path& csPath)
{
    if (!LoadShaderSpirVCode(csPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", csPath.string().c_str());
    }
    
    vkn::Shader shader;
    shader.Create(&s_vkDevice, VK_SHADER_STAGE_COMPUTE_BIT, s_shaderCodeBuffer).SetDebugName("BRDF_LUT_GEN_COMPUTE_SHADER");

    vkn::PSO& pso = s_PSOs[(size_t)PassID::BRDF_LUT_GEN];

    pso = s_computePSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .SetShader(shader)
        .SetLayout(s_PSOLayouts[(size_t)PassID::BRDF_LUT_GEN])
        .Build();

    pso.SetDebugName("BRDF_LUT_GEN_PSO");
}


static void CreateDbgDrawLinePipeline(const fs::path& vsPath, const fs::path& psPath)
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer).SetDebugName("DBG_DRAW_LINE_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer).SetDebugName("DBG_DRAW_LINE_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[(size_t)PassID::DBG_DRAW_LINES];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[(size_t)PassID::DBG_DRAW_LINES])
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(2.f)
    #ifdef ENG_REVERSED_Z
        .SetDepthTestState(VK_TRUE, VK_COMPARE_OP_GREATER_OR_EQUAL)
    #else
        .SetDepthTestState(VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
    #endif
        .SetDepthWriteState(VK_TRUE)
        .SetDepthBoundsTestState(VK_TRUE, 0.f, 1.f)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .AddColorAttachment(s_colorRT8U.GetFormat(), 
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_TRUE)
        .SetDepthAttachment(s_depthRT.GetFormat());
    
    pso = s_graphicsPSOBuilder.Build();
    
    pso.SetDebugName("DBG_DRAW_LINES_PSO");
#endif
}


static void CreateDbgDrawTrianglePipeline(const fs::path& vsPath, const fs::path& psPath)
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer).SetDebugName("DBG_DRAW_TRIANGLE_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer).SetDebugName("DBG_DRAW_TRIANGLE_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[(size_t)PassID::DBG_DRAW_TRIANGLES];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[(size_t)PassID::DBG_DRAW_TRIANGLES])
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(1.f)
    #ifdef ENG_REVERSED_Z
        .SetDepthTestState(VK_TRUE, VK_COMPARE_OP_GREATER_OR_EQUAL)
    #else
        .SetDepthTestState(VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
    #endif
        .SetDepthWriteState(VK_TRUE)
        .SetDepthBoundsTestState(VK_TRUE, 0.f, 1.f)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .AddColorAttachment(s_colorRT8U.GetFormat(), 
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_TRUE)
        .SetDepthAttachment(s_depthRT.GetFormat());
    
    pso = s_graphicsPSOBuilder.Build();
    
    pso.SetDebugName("DBG_DRAW_TRIANGLES_PSO");
#endif
}


static void CreateHZBGenPipeline(const fs::path& csPath)
{
    if (!LoadShaderSpirVCode(csPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", csPath.string().c_str());
    }
    
    vkn::Shader shader;
    shader.Create(&s_vkDevice, VK_SHADER_STAGE_COMPUTE_BIT, s_shaderCodeBuffer).SetDebugName("HZB_GEN_COMPUTE_SHADER");

    vkn::PSO& pso = s_PSOs[(size_t)PassID::HZB_GEN];

    pso = s_computePSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .SetShader(shader)
        .SetLayout(s_PSOLayouts[(size_t)PassID::HZB_GEN])
        .Build();

    pso.SetDebugName("HZB_GEN_PSO");
}


static void CreatePipelines()
{
    CreateGeomCullingOccludersPipelineLayout();
    CreateGeomCullingOcclusionPipelineLayout();
    CreateZPassPipelineLayout();
    CreateGBufferPipelineLayout();
    CreateDeferredLightingPipelineLayout();
    CreatePostProcessingPipelineLayout();
    CreateBackbufferPassPipelineLayout();
    CreateSkyboxPipelineLayout();
    CreateIrradianceMapGenPipelineLayout();
    CreatePrefilteredEnvMapGenPipelineLayout();
    CreateBRDFIntegrationLUTGenPipelineLayout();
    CreateDbgDrawLinePipelineLayout();
    CreateDbgDrawTrianglePipelineLayout();
    CreateHZBGenPipelineLayout();
    CreateGeomCullingOccludersPipeline(RND_SHADER_SPIRV_FULL_PATH("geom_culling_occluders.cs.spv"));
    CreateGeomCullingOcclusionPipeline(RND_SHADER_SPIRV_FULL_PATH("geom_culling_occlusion.cs.spv"));
    CreateZPassPipeline(RND_SHADER_SPIRV_FULL_PATH("zpass.vs.spv"), RND_SHADER_SPIRV_FULL_PATH("zpass.ps.spv"));
    CreateGBufferRenderPipeline(RND_SHADER_SPIRV_FULL_PATH("gbuffer.vs.spv"), RND_SHADER_SPIRV_FULL_PATH("gbuffer.ps.spv"));
    CreateDeferredLightingPipeline(RND_SHADER_SPIRV_FULL_PATH("deferred_lighting.vs.spv"), RND_SHADER_SPIRV_FULL_PATH("deferred_lighting.ps.spv"));
    CreatePostProcessingPipeline(RND_SHADER_SPIRV_FULL_PATH("post_processing.vs.spv"), RND_SHADER_SPIRV_FULL_PATH("post_processing.ps.spv"));
    CreateBackbufferPassPipeline(RND_SHADER_SPIRV_FULL_PATH("backbuffer.vs.spv"), RND_SHADER_SPIRV_FULL_PATH("backbuffer.ps.spv"));
    CreateSkyboxPipeline(RND_SHADER_SPIRV_FULL_PATH("skybox.vs.spv"), RND_SHADER_SPIRV_FULL_PATH("skybox.ps.spv"));
    CreateIrradianceMapGenPipeline(RND_SHADER_SPIRV_FULL_PATH("irradiance_map_gen.cs.spv"));
    CreatePrefilteredEnvMapGenPipeline(RND_SHADER_SPIRV_FULL_PATH("prefiltered_env_map_gen.cs.spv"));
    CreateBRDFIntegrationLUTGenPipeline(RND_SHADER_SPIRV_FULL_PATH("brdf_integration_gen.cs.spv"));
    CreateDbgDrawLinePipeline(RND_SHADER_SPIRV_FULL_PATH("dbg_draw_lines.vs.spv"), RND_SHADER_SPIRV_FULL_PATH("dbg_draw_lines.ps.spv"));
    CreateDbgDrawTrianglePipeline(RND_SHADER_SPIRV_FULL_PATH("dbg_draw_triangles.vs.spv"), RND_SHADER_SPIRV_FULL_PATH("dbg_draw_triangles.ps.spv"));
    CreateHZBGenPipeline(RND_SHADER_SPIRV_FULL_PATH("hzb.cs.spv"));
}


static void CreateCommonDbgTextures()
{
#ifndef ENG_BUILD_RELEASE
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

    texCreateInfos[(size_t)COMMON_DBG_TEX_IDX::CHECKERBOARD].extent = { 128u, 128u, 1u };

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
#ifndef ENG_BUILD_RELEASE
    auto UploadDbgTexture = [](vkn::CmdBuffer& cmdBuffer, size_t texIdx, size_t stagingBufIdx) -> void
    {
        vkn::Texture& texture = s_commonDbgTextures[texIdx];

        cmdBuffer
            .BeginBarrierList()
                .AddTextureBarrier(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
            .Push();

        vkn::BufferToTextureCopyInfo copyInfo = {};
        copyInfo.texSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyInfo.texSubresource.mipLevel = 0;
        copyInfo.texSubresource.baseArrayLayer = 0;
        copyInfo.texSubresource.layerCount = 1;
        copyInfo.texExtent = texture.GetSize();

        cmdBuffer.CmdCopyBuffer(s_commonStagingBuffers[stagingBufIdx], texture, copyInfo);
    
        cmdBuffer
            .BeginBarrierList()
                .AddTextureBarrier(texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
            .Push();
    };

    vkn::Buffer& redImageStagingBuffer = s_commonStagingBuffers[0];

    uint8_t* pRedImageData = (uint8_t*)redImageStagingBuffer.Map();
    pRedImageData[0] = 255;
    pRedImageData[1] = 0;
    pRedImageData[2] = 0;
    pRedImageData[3] = 255;
    redImageStagingBuffer.Unmap();

    vkn::Buffer& greenImageStagingBuffer = s_commonStagingBuffers[1];

    uint8_t* pGreenImageData = (uint8_t*)greenImageStagingBuffer.Map();
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

    uint8_t* pBlueImageData = (uint8_t*)blueImageStagingBuffer.Map();
    pBlueImageData[0] = 0;
    pBlueImageData[1] = 0;
    pBlueImageData[2] = 255;
    pBlueImageData[3] = 255;
    blueImageStagingBuffer.Unmap();

    vkn::Buffer& blackImageStagingBuffer = s_commonStagingBuffers[1];

    uint8_t* pBlackImageData = (uint8_t*)blackImageStagingBuffer.Map();
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

    uint8_t* pWhiteImageData = (uint8_t*)whiteImageStagingBuffer.Map();
    pWhiteImageData[0] = 255;
    pWhiteImageData[1] = 255;
    pWhiteImageData[2] = 255;
    pWhiteImageData[3] = 255;
    whiteImageStagingBuffer.Unmap();

    vkn::Buffer& greyImageStagingBuffer = s_commonStagingBuffers[1];

    uint8_t* pGreyImageData = (uint8_t*)greyImageStagingBuffer.Map();
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

    uint32_t* pCheckerboardImageData = (uint32_t*)checkerboardImageStagingBuffer.Map();

    const uint32_t whiteColorU32 = glm::packUnorm4x8(glm::float4(1.f));
    const uint32_t blackColorU32 = glm::packUnorm4x8(glm::float4(0.f, 0.f, 0.f, 1.f));

    for (uint32_t y = 0; y < checkerboardTex.GetSizeY(); ++y) {
        for (uint32_t x = 0; x < checkerboardTex.GetSizeX(); ++x) {
            const uint32_t idx = y * checkerboardTex.GetSizeX() + x;

            if (x < checkerboardTex.GetSizeX() / 2) {
                if (y < checkerboardTex.GetSizeY() / 2) {
                    pCheckerboardImageData[idx] = whiteColorU32;
                } else {
                    pCheckerboardImageData[idx] = blackColorU32;
                }
            } else {
                if (y < checkerboardTex.GetSizeY() / 2) {
                    pCheckerboardImageData[idx] = blackColorU32;
                } else {
                    pCheckerboardImageData[idx] = whiteColorU32;
                }
            }
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
    createInfo.size = sizeof(glm::uint) + MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(COMMON_CMD_DRAW_INDEXED_INDIRECT);
    createInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT | 
        VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT;
    createInfo.pAllocInfo = &allocInfo;

    s_commonOpaqueMeshDrawCmdBuffer.Create(createInfo).SetDebugName("COMMON_OPAQUE_MESH_DRAW_CMD_BUFFER");

    createInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    createInfo.size = MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(glm::uint);
    
    s_commonCulledOpaqueInstInfoIDsBuffer.Create(createInfo).SetDebugName("COMMON_CULLED_OPAQUE_INST_INFO_IDS_BUFFER");


    createInfo.size = sizeof(glm::uint) + MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(COMMON_CMD_DRAW_INDEXED_INDIRECT);
    createInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT | 
        VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT;

    s_commonAKillMeshDrawCmdBuffer.Create(createInfo).SetDebugName("COMMON_AKILL_MESH_DRAW_CMD_BUFFER");

    createInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    createInfo.size = MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(glm::uint);
    
    s_commonCulledAKillInstInfoIDsBuffer.Create(createInfo).SetDebugName("COMMON_CULLED_AKILL_INST_INFO_IDS_BUFFER");


    createInfo.size = sizeof(glm::uint) + MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(COMMON_CMD_DRAW_INDEXED_INDIRECT);
    createInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT | 
        VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT;

    s_commonTranspMeshDrawCmdBuffer.Create(createInfo).SetDebugName("COMMON_TRANSP_MESH_DRAW_CMD_BUFFER");

    createInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    createInfo.size = MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(glm::uint);
    
    s_commonCulledTranspInstInfoIDsBuffer.Create(createInfo).SetDebugName("COMMON_CULLED_TRANSP_INST_INFO_IDS_BUFFER");


    createInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    createInfo.size = (uint32_t)glm::ceil(s_cpuInstData.size() / 32.f);

    s_commonGeomVisFlagsBuffer.Create(createInfo).SetDebugName("COMMON_GEOM_VIS_FLAGS_BUFFER");
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
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::DEPTH, ZPASS_OPAQUE_INST_INFO_IDS_DESCRIPTOR_SLOT, 0, s_commonCulledOpaqueInstInfoIDsBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::DEPTH, ZPASS_AKILL_INST_INFO_IDS_DESCRIPTOR_SLOT, 0, s_commonCulledAKillInstInfoIDsBuffer);
}


static void WriteGeomCullingOccludersDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GEOM_CULLING_OCCLUDERS, 
        MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, 0, s_commonOpaqueMeshDrawCmdBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GEOM_CULLING_OCCLUDERS, 
        MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, 0, s_commonAKillMeshDrawCmdBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GEOM_CULLING_OCCLUDERS, 
        MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, 0, s_commonTranspMeshDrawCmdBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GEOM_CULLING_OCCLUDERS, 
        MESH_CULL_OPAQUE_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, 0, s_commonCulledOpaqueInstInfoIDsBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GEOM_CULLING_OCCLUDERS, 
        MESH_CULL_AKILL_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, 0, s_commonCulledAKillInstInfoIDsBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GEOM_CULLING_OCCLUDERS, 
        MESH_CULL_TRANSP_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, 0, s_commonCulledTranspInstInfoIDsBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GEOM_CULLING_OCCLUDERS, 
        MESH_CULL_VIS_FLAGS_UAV_DESCRIPTOR_SLOT, 0, s_commonGeomVisFlagsBuffer);
}


static void WriteGeomCullingOcclusionDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GEOM_CULLING_OCCLUSION, 
        MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, 0, s_commonOpaqueMeshDrawCmdBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GEOM_CULLING_OCCLUSION, 
        MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, 0, s_commonAKillMeshDrawCmdBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GEOM_CULLING_OCCLUSION, 
        MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, 0, s_commonTranspMeshDrawCmdBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GEOM_CULLING_OCCLUSION, 
        MESH_CULL_OPAQUE_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, 0, s_commonCulledOpaqueInstInfoIDsBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GEOM_CULLING_OCCLUSION, 
        MESH_CULL_AKILL_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, 0, s_commonCulledAKillInstInfoIDsBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GEOM_CULLING_OCCLUSION, 
        MESH_CULL_TRANSP_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, 0, s_commonCulledTranspInstInfoIDsBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GEOM_CULLING_OCCLUSION, 
        MESH_CULL_VIS_FLAGS_UAV_DESCRIPTOR_SLOT, 0, s_commonGeomVisFlagsBuffer);
}


static void WriteGBufferDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GBUFFER, GBUFFER_OPAQUE_INST_INFO_IDS_DESCRIPTOR_SLOT, 0, s_commonCulledOpaqueInstInfoIDsBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GBUFFER, GBUFFER_AKILL_INST_INFO_IDS_DESCRIPTOR_SLOT, 0, s_commonCulledAKillInstInfoIDsBuffer);
}


static void WriteDeferredLightingDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::DEFERRED_LIGHTING, DEFERRED_LIGHTING_OUTPUT_UAV_DESCRIPTOR_SLOT, 0, s_colorRTView16F);

    std::array<vkn::TextureView*, GBUFFER_RT_COUNT> gbufferViews = {};
    for (size_t i = 0; i < GBUFFER_RT_COUNT; ++i) {
        gbufferViews[i] = &s_gbufferRTViews[i];
    }

    for (size_t i = 0; i < GBUFFER_RT_COUNT; ++i) {
        s_descriptorBuffer.WriteDescriptor((size_t)PassID::DEFERRED_LIGHTING, DEFERRED_LIGHTING_GBUFFER_0_DESCRIPTOR_SLOT + i, 0, *gbufferViews[i]);
    }

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::DEFERRED_LIGHTING, DEFERRED_LIGHTING_DEPTH_DESCRIPTOR_SLOT, 0, s_depthRTView);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::DEFERRED_LIGHTING, DEFERRED_LIGHTING_IRRADIANCE_MAP_DESCRIPTOR_SLOT, 0, s_irradianceMapTextureView);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::DEFERRED_LIGHTING, DEFERRED_LIGHTING_PREFILTERED_ENV_MAP_DESCRIPTOR_SLOT, 0, s_prefilteredEnvMapTextureView);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::DEFERRED_LIGHTING, DEFERRED_LIGHTING_BRDF_LUT_DESCRIPTOR_SLOT, 0, s_brdfLUTTextureView);
}


static void WritePostProcessingDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::POST_PROCESSING, POST_PROCESSING_INPUT_COLOR_DESCRIPTOR_SLOT, 0, s_colorRTView16F);
}


static void WriteBackbufferPassDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::BACKBUFFER, BACKBUFFER_INPUT_COLOR_DESCRIPTOR_SLOT, 0, s_colorRTView8U);
}


static void WriteSkyboxDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::SKYBOX, SKYBOX_TEXTURE_DESCRIPTOR_SLOT, 0, s_skyboxTextureView);
}


static void WriteIrradianceMapGenDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::IRRADIANCE_MAP_GEN, IRRADIANCE_MAP_GEN_ENV_MAP_DESCRIPTOR_SLOT, 0, s_skyboxTextureView);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::IRRADIANCE_MAP_GEN, IRRADIANCE_MAP_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT, 0, s_irradianceMapTextureViewRW);
}


static void WritePrefilteredEnvMapGenDescriptorSets()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::PREFILT_ENV_MAP_GEN, PREFILTERED_ENV_MAP_GEN_ENV_MAP_DESCRIPTOR_SLOT, 0, s_skyboxTextureView);
    
    for (uint32_t i = 0; i < s_prefilteredEnvMapTextureViewRWs.size(); ++i) {
        s_descriptorBuffer.WriteDescriptor((size_t)PassID::PREFILT_ENV_MAP_GEN, PREFILTERED_ENV_MAP_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT, i, s_prefilteredEnvMapTextureViewRWs[i]);
    }
}


static void WriteBRDFIntegrationLUTGenDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::BRDF_LUT_GEN, BRDF_INTEGRATION_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT, 0, s_brdfLUTTextureViewRW);
}


static void WriteHZBGenDescriptorSets()
{
    for (uint32_t i = 0; i < s_HZB.GetMipCount(); ++i) {
        vkn::TextureView& mip = s_HZBMipViews[i];

        s_descriptorBuffer.WriteDescriptor((size_t)PassID::HZB_GEN, HZB_SRC_MIPS_DESCRIPTOR_SLOT, i, mip);
        s_descriptorBuffer.WriteDescriptor((size_t)PassID::HZB_GEN, HZB_DST_MIPS_UAV_DESCRIPTOR_SLOT, i, mip);
    }

    // First source mip must contain original depth buffer
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::HZB_GEN, HZB_SRC_MIPS_DESCRIPTOR_SLOT, 0, s_depthRTView);
}


static void WriteCommonDescriptorSet()
{
    for (size_t i = 0; i < s_commonSamplers.size(); ++i) {
        s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON, COMMON_SAMPLERS_DESCRIPTOR_SLOT, i, s_commonSamplers[i]);
    }

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON, COMMON_CONST_BUFFER_DESCRIPTOR_SLOT, 0, s_commonConstBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON, COMMON_MESH_DATA_BUFFER_DESCRIPTOR_SLOT, 0, s_commonMeshDataBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON, COMMON_TRANSFORMS_DESCRIPTOR_SLOT, 0, s_commonTransformDataBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON, COMMON_MATERIALS_DESCRIPTOR_SLOT, 0, s_commonMaterialDataBuffer);

    for (size_t i = 0; i < s_commonMaterialTextureViews.size(); ++i) {
        s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON, COMMON_MTL_TEXTURES_DESCRIPTOR_SLOT, i, s_commonMaterialTextureViews[i]);
    }

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON,COMMON_INST_DATA_BUFFER_DESCRIPTOR_SLOT, 0, s_commonInstDataBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON, COMMON_VERTEX_DATA_DESCRIPTOR_SLOT, 0, s_vertexBuffer);

#ifndef ENG_BUILD_RELEASE
    for (size_t i = 0; i < s_commonDbgTextureViews.size(); ++i) {
        s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON, COMMON_DBG_TEXTURES_DESCRIPTOR_SLOT, i, s_commonDbgTextureViews[i]);
    }
#endif

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON, COMMON_AABB_BUFFER_DESCRIPTOR_SLOT, 0, s_commonAABBDataBuffer);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON, COMMON_DEPTH_DESCRIPTOR_SLOT, 0, s_depthRTView);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON, COMMON_HZB_DESCRIPTOR_SLOT, 0, s_HZBView);
}


static void WriteDbgDrawLineDescriptorSet()
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::DBG_DRAW_LINES, DBG_DRAW_LINES_VERTEX_BUFFER_DESCRIPTOR_SLOT, 0, s_dbgLineVertexDataGPU);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::DBG_DRAW_LINES, DBG_DRAW_LINES_DATA_DESCRIPTOR_SLOT, 0, s_dbgLineDataGPU);
#endif
}


static void WriteDbgDrawTriangleDescriptorSet()
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::DBG_DRAW_TRIANGLES, DBG_DRAW_TRIANGLES_VERTEX_BUFFER_DESCRIPTOR_SLOT, 0, s_dbgTriangleVertexDataGPU);
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::DBG_DRAW_TRIANGLES, DBG_DRAW_TRIANGLES_DATA_DESCRIPTOR_SLOT, 0, s_dbgTriangleDataGPU);
#endif
}


static void WriteDescriptorSets()
{
    WriteCommonDescriptorSet();
    WriteZPassDescriptorSet();
    WriteGeomCullingOccludersDescriptorSet();
    WriteGeomCullingOcclusionDescriptorSet();
    WriteGBufferDescriptorSet();
    WriteDeferredLightingDescriptorSet();
    WritePostProcessingDescriptorSet();
    WriteBackbufferPassDescriptorSet();
    WriteSkyboxDescriptorSet();
    WriteIrradianceMapGenDescriptorSet();
    WritePrefilteredEnvMapGenDescriptorSets();
    WriteBRDFIntegrationLUTGenDescriptorSet();
    WriteDbgDrawLineDescriptorSet();
    WriteDbgDrawTriangleDescriptorSet();
    WriteHZBGenDescriptorSets();
}


static void LoadSceneMeshData(const gltf::Asset& asset)
{
    ENG_PROFILE_SCOPED_MARKER_C("Load_Scene_Mesh_Data", eng::ProfileColor::DarkMagenta);

    eng::Timer timer;

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

            COMMON_MESH_DATA cpuMesh = {};

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

            cpuMesh.BOUNDS_MIN_LCS = minVert;
            cpuMesh.BOUNDS_MAX_LCS = maxVert;

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
    ENG_PROFILE_SCOPED_MARKER_C("Load_Scene_Textures_Data", eng::ProfileColor::DarkMagenta);

    eng::Timer timer;

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
    ENG_PROFILE_SCOPED_MARKER_C("Load_Scene_Material_Data", eng::ProfileColor::DarkMagenta);

    eng::Timer timer;

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
    ENG_PROFILE_SCOPED_MARKER_C("Load_Scene_Inst_Data", eng::ProfileColor::DarkMagenta);

    eng::Timer timer;

    size_t meshInstCount = 0;

    for (size_t sceneId = 0; sceneId < asset.scenes.size(); ++sceneId) {
        gltf::iterateSceneNodes(asset, sceneId, gltf::math::fmat4x4(1.f), [&meshInstCount, &asset](auto&& node, auto&& trs)
        {    
            if (node.meshIndex.has_value()) {
                const uint32_t meshIdx = node.meshIndex.value();
                meshInstCount += asset.meshes[meshIdx].primitives.size();
            }
        });
    }

    std::vector<uint32_t> meshBaseIdxOffsets(asset.meshes.size());
    for (size_t i = 1; i < meshBaseIdxOffsets.size(); ++i) {
        meshBaseIdxOffsets[i] = meshBaseIdxOffsets[i - 1] + asset.meshes[i - 1].primitives.size();
    }

    s_cpuInstData.reserve(meshInstCount);
    s_cpuInstData.clear();

    s_cpuTransformData.reserve(asset.nodes.size());
    s_cpuTransformData.clear();

    s_cpuAABBData.reserve(meshInstCount);
    s_cpuAABBData.clear();

    auto PushAABB = [&](uint32_t meshIdx, const glm::float4x4& wMatr) -> uint32_t
    {
        const COMMON_MESH_DATA& mesh = s_cpuMeshData[meshIdx];

        const math::AABB aabb = GetWorldAABB(math::AABB(mesh.BOUNDS_MIN_LCS, mesh.BOUNDS_MAX_LCS), wMatr);

        s_cpuAABBData.emplace_back(aabb.min, aabb.max);

        return s_cpuAABBData.size() - 1;
    };

    for (size_t sceneId = 0; sceneId < asset.scenes.size(); ++sceneId) {
        gltf::iterateSceneNodes(asset, sceneId, gltf::math::fmat4x4(1.f), [&](auto&& node, auto&& trs)
        {
            static_assert(sizeof(trs) == sizeof(glm::float4x4));
    
            glm::float4x4 transform(1.f);
            memcpy(&transform, &trs, sizeof(transform));

            // if (!math::IsZero(math::GetTranslation(transform).x) || !math::IsZero(math::GetTranslation(transform).y)) {
            //     return;
            // }

            s_cpuTransformData.emplace_back(transform);
    
            if (node.meshIndex.has_value()) {
                const uint32_t trsIdx = s_cpuTransformData.size() - 1;
                
                const uint32_t meshIdx = node.meshIndex.value();
                const gltf::Mesh& mesh = asset.meshes[meshIdx];

                const uint32_t baseIdx = meshBaseIdxOffsets[meshIdx];

                for (uint32_t i = 0; i < mesh.primitives.size(); ++i) {
                    const gltf::Primitive& primitive = mesh.primitives[i];

                    COMMON_INST_DATA instData = {};
                    
                    instData.TRANSFORM_IDX = trsIdx;
                    instData.MESH_IDX = baseIdx + i;
    
                    CORE_ASSERT_MSG(primitive.materialIndex.has_value(), "Some of mesh %s primitive doesn't have material", mesh.name.c_str());
                    instData.MATERIAL_IDX = primitive.materialIndex.value();

                    instData.VOLUME_IDX = PushAABB(instData.MESH_IDX, transform);

                    s_cpuInstData.emplace_back(instData);
                }
            }
        });
    }

    CORE_LOG_INFO("FastGLTF: Instance data loading finished: %f ms", timer.End().GetDuration<float, std::milli>());

    timer.Reset().Start();

    std::sort(s_cpuInstData.begin(), s_cpuInstData.end(), 
        [](const COMMON_INST_DATA& a, const COMMON_INST_DATA& b) {
            return a.MESH_IDX < b.MESH_IDX;
        }
    );

    CORE_LOG_INFO("Instance data sorting finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void UploadGPUMeshData()
{
    ENG_PROFILE_SCOPED_MARKER_C("Upload_GPU_Mesh_Data", eng::ProfileColor::DarkMagenta);

    eng::Timer timer;

    vkn::Buffer& stagingVertBuffer = s_commonStagingBuffers[0];

    const size_t gpuVertBufferSize = s_cpuVertexBuffer.size() * sizeof(Vertex);
    CORE_ASSERT(gpuVertBufferSize <= stagingVertBuffer.GetMemorySize());

    void* pVertexBufferData = stagingVertBuffer.Map();
    memcpy(pVertexBufferData, s_cpuVertexBuffer.data(), gpuVertBufferSize);
    stagingVertBuffer.Unmap();

    vkn::Buffer& stagingIndexBuffer = s_commonStagingBuffers[1];

    const size_t gpuIndexBufferSize = s_cpuIndexBuffer.size() * sizeof(IndexType);
    CORE_ASSERT(gpuIndexBufferSize <= stagingIndexBuffer.GetMemorySize());

    void* pIndexBufferData = stagingIndexBuffer.Map();
    memcpy(pIndexBufferData, s_cpuIndexBuffer.data(), gpuIndexBufferSize);
    stagingIndexBuffer.Unmap();

    vkn::AllocationInfo vertBufAllocInfo = {};
    vertBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    vertBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo vertBufCreateInfo = {};
    vertBufCreateInfo.pDevice = &s_vkDevice;
    vertBufCreateInfo.size = gpuVertBufferSize;
    vertBufCreateInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    vertBufCreateInfo.pAllocInfo = &vertBufAllocInfo;

    s_vertexBuffer.Create(vertBufCreateInfo).SetDebugName("COMMON_VB");

    vkn::AllocationInfo idxBufAllocInfo = {};
    idxBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    idxBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo idxBufCreateInfo = {};
    idxBufCreateInfo.pDevice = &s_vkDevice;
    idxBufCreateInfo.size = gpuIndexBufferSize;
    idxBufCreateInfo.usage = VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT;
    idxBufCreateInfo.pAllocInfo = &idxBufAllocInfo;

    s_indexBuffer.Create(idxBufCreateInfo).SetDebugName("COMMON_IB");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        cmdBuffer.CmdCopyBuffer(stagingVertBuffer, s_vertexBuffer, gpuVertBufferSize);
        cmdBuffer.CmdCopyBuffer(stagingIndexBuffer, s_indexBuffer, gpuIndexBufferSize);    
    });

    vkn::Buffer& stagingMeshInfosBuffer = s_commonStagingBuffers[0];

    const size_t meshDataBufferSize = s_cpuMeshData.size() * sizeof(COMMON_MESH_DATA);
    CORE_ASSERT(meshDataBufferSize <= stagingMeshInfosBuffer.GetMemorySize());

    void* pMeshBufferData = stagingMeshInfosBuffer.Map();
    memcpy(pMeshBufferData, s_cpuMeshData.data(), meshDataBufferSize);
    stagingMeshInfosBuffer.Unmap();

    vkn::AllocationInfo meshInfosBufAllocInfo = {};
    meshInfosBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    meshInfosBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo meshInfosBufCreateInfo = {};
    meshInfosBufCreateInfo.pDevice = &s_vkDevice;
    meshInfosBufCreateInfo.size = meshDataBufferSize;
    meshInfosBufCreateInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    meshInfosBufCreateInfo.pAllocInfo = &meshInfosBufAllocInfo;
    
    s_commonMeshDataBuffer.Create(meshInfosBufCreateInfo).SetDebugName("COMMON_MESH_DATA");

    vkn::Buffer& stagingTransformDataBuffer = s_commonStagingBuffers[1];

    const size_t trsDataBufferSize = s_cpuTransformData.size() * sizeof(s_cpuTransformData[0]);
    CORE_ASSERT(trsDataBufferSize <= stagingTransformDataBuffer.GetMemorySize());

    void* pData = stagingTransformDataBuffer.Map();
    memcpy(pData, s_cpuTransformData.data(), trsDataBufferSize);
    stagingTransformDataBuffer.Unmap();

    vkn::AllocationInfo commonTrsBufAllocInfo = {};
    commonTrsBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    commonTrsBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo commonTrsBufCreateInfo = {};
    commonTrsBufCreateInfo.pDevice = &s_vkDevice;
    commonTrsBufCreateInfo.size = trsDataBufferSize;
    commonTrsBufCreateInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    commonTrsBufCreateInfo.pAllocInfo = &commonTrsBufAllocInfo;

    s_commonTransformDataBuffer.Create(commonTrsBufCreateInfo).SetDebugName("COMMON_TRANSFORM_DATA");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        cmdBuffer.CmdCopyBuffer(stagingMeshInfosBuffer, s_commonMeshDataBuffer, meshDataBufferSize);
        cmdBuffer.CmdCopyBuffer(stagingTransformDataBuffer, s_commonTransformDataBuffer, trsDataBufferSize);
    });

    CORE_LOG_INFO("FastGLTF: Mesh data GPU upload finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void UploadGPUTextureData()
{
    ENG_PROFILE_SCOPED_MARKER_C("Upload_GPU_Texture_Data", eng::ProfileColor::DarkMagenta);

    eng::Timer timer;

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

            void* pImageData = stagingTexBuffer.Map();
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

                cmdBuffer
                    .BeginBarrierList()
                        .AddTextureBarrier(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT, 
                            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1)
                    .Push();

                vkn::BufferToTextureCopyInfo copyInfo = {};
                copyInfo.texSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyInfo.texSubresource.mipLevel = 0;
                copyInfo.texSubresource.baseArrayLayer = 0;
                copyInfo.texSubresource.layerCount = 1;
                copyInfo.texExtent = texture.GetSize();

                cmdBuffer.CmdCopyBuffer(s_commonStagingBuffers[j], texture, copyInfo);

                const TextureLoadData& texData = s_cpuTexturesData[textureIdx];

                GenerateTextureMipmaps(cmdBuffer, texture, texData);

                cmdBuffer
                    .BeginBarrierList()
                        .AddTextureBarrier(texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, 
                            VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
                    .Push();
            }
        });
    }
}


static void UploadGPUMaterialData()
{
    ENG_PROFILE_SCOPED_MARKER_C("Upload_GPU_Material_Data", eng::ProfileColor::DarkMagenta);

    eng::Timer timer;

    vkn::Buffer& stagingMtlDataBuffer = s_commonStagingBuffers[0];

    const size_t mtlDataBufferSize = s_cpuMaterialData.size() * sizeof(COMMON_MATERIAL);
    CORE_ASSERT(mtlDataBufferSize <= stagingMtlDataBuffer.GetMemorySize());

    void* pData = stagingMtlDataBuffer.Map();
    memcpy(pData, s_cpuMaterialData.data(), mtlDataBufferSize);
    stagingMtlDataBuffer.Unmap();

    vkn::AllocationInfo mtlBufAllocInfo = {};
    mtlBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    mtlBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo mtlBufCreateInfo = {};
    mtlBufCreateInfo.pDevice = &s_vkDevice;
    mtlBufCreateInfo.size = mtlDataBufferSize;
    mtlBufCreateInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    mtlBufCreateInfo.pAllocInfo = &mtlBufAllocInfo;

    s_commonMaterialDataBuffer.Create(mtlBufCreateInfo).SetDebugName("COMMON_MATERIAL_DATA");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        cmdBuffer.CmdCopyBuffer(stagingMtlDataBuffer, s_commonMaterialDataBuffer, mtlDataBufferSize);
    });

    CORE_LOG_INFO("FastGLTF: Material data GPU upload finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void UploadGPUInstData()
{
    ENG_PROFILE_SCOPED_MARKER_C("Upload_GPU_Inst_Data", eng::ProfileColor::DarkMagenta);

    eng::Timer timer;

    vkn::Buffer& instStagingBuffer = s_commonStagingBuffers[0];

    const size_t instBufferSize = s_cpuInstData.size() * sizeof(COMMON_INST_DATA);
    CORE_ASSERT(instBufferSize <= instStagingBuffer.GetMemorySize());

    {
        void* pData = instStagingBuffer.Map();
        memcpy(pData, s_cpuInstData.data(), instBufferSize);
        instStagingBuffer.Unmap();
    }

    vkn::AllocationInfo instInfosBufAllocInfo = {};
    instInfosBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    instInfosBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo instInfosBufCreateInfo = {};
    instInfosBufCreateInfo.pDevice = &s_vkDevice;
    instInfosBufCreateInfo.size = instBufferSize;
    instInfosBufCreateInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    instInfosBufCreateInfo.pAllocInfo = &instInfosBufAllocInfo;

    s_commonInstDataBuffer.Create(instInfosBufCreateInfo).SetDebugName("COMMON_INSTANCE_DATA");

    vkn::Buffer& aabbStagingBuffer = s_commonStagingBuffers[1];

    const size_t aabbBufferSize = s_cpuAABBData.size() * sizeof(COMMON_INST_AABB);
    CORE_ASSERT(aabbBufferSize <= aabbStagingBuffer.GetMemorySize());

    {
        void* pData = aabbStagingBuffer.Map();
        memcpy(pData, s_cpuAABBData.data(), aabbBufferSize);
        aabbStagingBuffer.Unmap();
    }

    vkn::AllocationInfo aabbBufAllocInfo = {};
    aabbBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    aabbBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo aabbBufCreateInfo = {};
    aabbBufCreateInfo.pDevice = &s_vkDevice;
    aabbBufCreateInfo.size = aabbBufferSize;
    aabbBufCreateInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    aabbBufCreateInfo.pAllocInfo = &aabbBufAllocInfo;

    s_commonAABBDataBuffer.Create(aabbBufCreateInfo).SetDebugName("COMMON_AABB_DATA");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        cmdBuffer.CmdCopyBuffer(instStagingBuffer, s_commonInstDataBuffer, instBufferSize);
        cmdBuffer.CmdCopyBuffer(aabbStagingBuffer, s_commonAABBDataBuffer, aabbBufferSize);
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
    
    ENG_PROFILE_SCOPED_MARKER_C("Load_Scene", eng::ProfileColor::DarkMagenta);
    
    eng::Timer timer;

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
    s_commonConstBuffer
        .CreateConstBuffer(&s_vkDevice, sizeof(COMMON_CB_DATA), VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT).SetDebugName("COMMON_CB");
}


void UpdateGPUCommonConstBuffer()
{
    ENG_PROFILE_SCOPED_MARKER_C("Update_Common_Const_Buffer", eng::ProfileColor::Cyan4);

    COMMON_CB_DATA& constBuff = *reinterpret_cast<COMMON_CB_DATA*>(s_commonConstBuffer.Map());

    const glm::float4x4& viewMatrix = s_camera.GetViewMatrix();
    const glm::float4x4& projMatrix = s_camera.GetProjMatrix();
    const glm::float4x4& viewProjMatrix = s_camera.GetViewProjMatrix();

    constBuff.VIEW_MATRIX = viewMatrix;
    constBuff.PROJ_MATRIX = projMatrix;
    constBuff.VIEW_PROJ_MATRIX = viewProjMatrix;

    constBuff.INV_VIEW_MATRIX = glm::inverse(viewMatrix);
    constBuff.INV_PROJ_MATRIX = glm::inverse(projMatrix);
    constBuff.INV_VIEW_PROJ_MATRIX = glm::inverse(viewProjMatrix);

    const math::Frustum& camFrustum = s_camera.GetFrustum();

    for (size_t i = 0; i <  math::Frustum::PLANE_COUNT; ++i) {
        const math::Plane& srcPlane = camFrustum.GetPlane(i);
        PLANE& dstPlane = constBuff.CAMERA_FRUSTUM.planes[i];

        dstPlane.normal = srcPlane.normal;
        dstPlane.distance = srcPlane.distance;
    }

    if (s_cullingTestMode) {
        for (size_t i = 0; i <  math::Frustum::PLANE_COUNT; ++i) {
            const math::Plane& srcPlane = s_fixedCamCullFrustum.GetPlane(i);
            PLANE& dstPlane = constBuff.CULLING_CAMERA_FRUSTUM.planes[i];

            dstPlane.normal = srcPlane.normal;
            dstPlane.distance = srcPlane.distance;
        }

        constBuff.CULLING_VIEW_PROJ_MATRIX = s_fixedCamCullViewProjMatr;
    } else {
        constBuff.CULLING_CAMERA_FRUSTUM = constBuff.CAMERA_FRUSTUM;
        constBuff.CULLING_VIEW_PROJ_MATRIX = constBuff.VIEW_PROJ_MATRIX;
    }
    
    constBuff.SCREEN_SIZE.x = static_cast<float>(s_pWnd->GetWidth());
    constBuff.SCREEN_SIZE.y = static_cast<float>(s_pWnd->GetHeight());

    constBuff.Z_NEAR = s_camera.GetZNear();
    constBuff.Z_FAR = s_camera.GetZFar();

    uint32_t dbgFlags = 0;
    dbgFlags |= TONEMAPPING_MASKS[s_tonemappingPreset];
    dbgFlags |= s_useMeshIndirectDraw        ? (uint32_t)COMMON_DBG_FLAG_MASKS::USE_MESH_INDIRECT_DRAW_MASK : 0;
    dbgFlags |= s_useIndirectLighting        ? (uint32_t)COMMON_DBG_FLAG_MASKS::USE_INDIRECT_LIGHTING_MASK : 0;
    dbgFlags |= s_useMeshCulling && s_useMeshFrustumCulling      ? (uint32_t)COMMON_DBG_FLAG_MASKS::USE_MESH_GPU_FRUSTUM_CULLING_MASK : 0;
    dbgFlags |= s_useMeshCulling && s_useMeshContributionCulling ? (uint32_t)COMMON_DBG_FLAG_MASKS::USE_MESH_GPU_CONTRIBUTION_CULLING_MASK : 0;
    dbgFlags |= s_useMeshCulling && s_useMeshHZBCulling          ? (uint32_t)COMMON_DBG_FLAG_MASKS::USE_MESH_GPU_HZB_CULLING_MASK : 0;

    constBuff.DBG_FLAGS = dbgFlags;
    constBuff.DBG_VIS_FLAGS = DBG_RT_OUTPUT_MASKS[s_dbgOutputRTIdx];
    
    constBuff.CAM_WPOS = glm::float4(s_camera.GetPosition(), 0.f);

    s_commonConstBuffer.Unmap();
}


static void ResizeVkSwapchain(eng::Window& window)
{
    bool resizeSucceded;
    s_vkSwapchain.Resize(window.GetWidth(), window.GetHeight(), resizeSucceded);
    
    s_swapchainRecreateRequired = !resizeSucceded;
}


void UpdateScene()
{
    ClearDebugDrawData();

    if (s_swapchainRecreateRequired) {
        s_vkDevice.WaitIdle();

        ResizeVkSwapchain(*s_pWnd);

        if (s_swapchainRecreateRequired) {
            s_skipRender = true;
        } else {
            ResizeDynamicRenderTargets();
            WriteDescriptorSets();
        }    
    }

    const float moveDist = glm::length(s_cameraVel);

    if (!math::IsZero(moveDist)) {
        const glm::float3 moveDir = glm::normalize(s_camera.GetRotation() * (s_cameraVel / moveDist));
        s_camera.MoveAlongDir(moveDir, moveDist);
    }

    s_camera.Update();

    if (s_drawInstAABBs) {
        const math::Frustum& frustum = s_camera.GetFrustum();

        static constexpr glm::float4 COLOR = glm::float4(1.f, 1.f, 0.f, 1.f);

        for (const COMMON_INST_AABB& volume : s_cpuAABBData) {
            const math::AABB aabb = volume.GetAABB(); 

            if (frustum.IsIntersect(aabb)) {
                RenderDebugAABBWired(math::MakeTS(aabb.GetCenter(), aabb.GetSize()), COLOR);
            }
        }
    }

    if (s_cullingTestMode) {
        RenderDebugFrustumFilled(s_fixedCamCullInvViewProjMatr, glm::float4(0.5f, 0.5f, 0.5f, 0.35f));
        RenderDebugFrustumWired(s_fixedCamCullInvViewProjMatr, glm::float4(1.f));
    }
}


void PresentImage(uint32_t imageIndex)
{
    ENG_PROFILE_SCOPED_MARKER_C("Present_Swapchain_Image", eng::ProfileColor::Maroon3);

    const VkResult presentResult = s_vkDevice.GetQueue().Present(s_vkSwapchain, imageIndex, &s_renderFinishedSemaphores[imageIndex]);

    if (presentResult != VK_SUBOPTIMAL_KHR && presentResult != VK_ERROR_OUT_OF_DATE_KHR) {
        VK_CHECK(presentResult);
    } else {
        s_swapchainRecreateRequired = !s_pWnd->IsMinimized() && s_pWnd->GetWidth() != 0 &&  s_pWnd->GetHeight() != 0;
    }
}


static void PrecomputeIBLIrradianceMap(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Precompute_IBL_Irradiance_Map", eng::ProfileColor::OrangeRed);
    eng::Timer timer;

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_irradianceMapTexture, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
        .Push();

    vkn::PSO& pso = s_PSOs[(size_t)PassID::IRRADIANCE_MAP_GEN];

    cmdBuffer.CmdBindPSO(pso);
    
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::IRRADIANCE_MAP_GEN, .shaderSetIdx = DESC_SET_PER_DRAW });

    IRRADIANCE_MAP_PER_DRAW_DATA pushConsts = {};
    pushConsts.ENV_MAP_FACE_SIZE.x = s_skyboxTexture.GetSizeX();
    pushConsts.ENV_MAP_FACE_SIZE.y = s_skyboxTexture.GetSizeY();

    cmdBuffer.CmdPushConstants(pso, VK_SHADER_STAGE_COMPUTE_BIT, pushConsts);

    cmdBuffer.CmdDispatch(ceil(COMMON_IRRADIANCE_MAP_SIZE.x / 32.f), ceil(COMMON_IRRADIANCE_MAP_SIZE.y / 32.f), 6);

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_irradianceMapTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
        .Push();

    CORE_LOG_INFO("Irradiance map generation finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void PrecomputeIBLPrefilteredEnvMap(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Precompute_IBL_Prefiltered_Env_Map", eng::ProfileColor::OrangeRed);
    eng::Timer timer;

    vkn::PSO& pso = s_PSOs[(size_t)PassID::PREFILT_ENV_MAP_GEN];

    cmdBuffer.CmdBindPSO(pso);

    PREFILTERED_ENV_MAP_PER_DRAW_DATA pushConsts = {};
    pushConsts.ENV_MAP_FACE_SIZE.x = s_skyboxTexture.GetSizeX();
    pushConsts.ENV_MAP_FACE_SIZE.y = s_skyboxTexture.GetSizeY();

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_prefilteredEnvMapTexture, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
            .Push();

    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::PREFILT_ENV_MAP_GEN, .shaderSetIdx = DESC_SET_PER_DRAW });

    for (size_t mip = 0; mip < COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT; ++mip) {
        pushConsts.MIP = mip;

        cmdBuffer.CmdPushConstants(pso, VK_SHADER_STAGE_COMPUTE_BIT, pushConsts);

        const uint32_t sizeX = COMMON_PREFILTERED_ENV_MAP_SIZE.x >> mip;
        const uint32_t sizeY = COMMON_PREFILTERED_ENV_MAP_SIZE.y >> mip;

        cmdBuffer.CmdDispatch((uint32_t)ceil(sizeX / 32.f), (uint32_t)ceil(sizeY / 32.f), 6U);
    }

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_prefilteredEnvMapTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
        .Push();

    CORE_LOG_INFO("Prefiltered env map generation finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void PrecomputeIBLBRDFIntergrationLUT(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Precompute_IBL_BRDF_Intergration_LUT", eng::ProfileColor::OrangeRed);
    eng::Timer timer;

    vkn::PSO& pso = s_PSOs[(size_t)PassID::BRDF_LUT_GEN];

    cmdBuffer.CmdBindPSO(pso);

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_brdfLUTTexture, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
        .Push();

    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::BRDF_LUT_GEN, .shaderSetIdx = DESC_SET_PER_DRAW });

    cmdBuffer.CmdDispatch((uint32_t)ceil(COMMON_BRDF_INTEGRATION_LUT_SIZE.x / 32.f), (uint32_t)ceil(COMMON_BRDF_INTEGRATION_LUT_SIZE.y / 32.f), 1u);

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_brdfLUTTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
        .Push();

    CORE_LOG_INFO("BRDF LUT generation finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void HZBGeneratePass(vkn::CmdBuffer& cmdBuffer)
{
    if (s_cullingTestMode) {
        return;
    }

    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "HZB_Generation_Pass", eng::ProfileColor::Grey80);
    eng::Timer timer;

    vkn::PSO& pso = s_PSOs[(size_t)PassID::HZB_GEN];
    
    cmdBuffer.CmdBindPSO(pso);

    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::HZB_GEN, .shaderSetIdx = DESC_SET_PER_DRAW });

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_depthRT, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_DEPTH_BIT)
            .AddTextureBarrier(s_HZB, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1)
        .Push();

    glm::uvec2 dstMipSize = glm::uvec2(s_HZB.GetSizeX(), s_HZB.GetSizeY());

    HZB_GEN_PER_DRAW_DATA pushConsts = {};
    pushConsts.DST_MIP_RESOLUTION = dstMipSize;
    pushConsts.DST_MIP_IDX = 0;

    cmdBuffer.CmdPushConstants(pso, VK_SHADER_STAGE_COMPUTE_BIT, pushConsts);

    cmdBuffer.CmdDispatch(
        (uint32_t)glm::ceil(s_HZB.GetSizeX() / (float)HZB_BUILD_CS_GROUP_SIZE), 
        (uint32_t)glm::ceil(s_HZB.GetSizeY() / (float)HZB_BUILD_CS_GROUP_SIZE),
        1u
    );

    for (uint32_t mip = 1; mip < s_HZB.GetMipCount(); ++mip) {
        dstMipSize = glm::max(dstMipSize >> 1u, ONEU2);

        cmdBuffer
            .BeginBarrierList()
                .AddTextureBarrier(s_HZB, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                    VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, 1)
                .AddTextureBarrier(s_HZB, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                    VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, mip, 1)
            .Push();

        pushConsts.DST_MIP_RESOLUTION = dstMipSize;
        pushConsts.DST_MIP_IDX = mip;

        cmdBuffer.CmdPushConstants(pso, VK_SHADER_STAGE_COMPUTE_BIT, pushConsts);

        cmdBuffer.CmdDispatch(
            (uint32_t)glm::ceil(dstMipSize.x / (float)HZB_BUILD_CS_GROUP_SIZE), 
            (uint32_t)glm::ceil(dstMipSize.y / (float)HZB_BUILD_CS_GROUP_SIZE),
            1u
        );
    }

    // To get all mips in same layout
    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_HZB, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, s_HZB.GetMipCount() - 1, 1)
        .Push();
}


static void GeomCullingClearCmdBuffers(vkn::CmdBuffer& cmdBuffer)
{
    cmdBuffer
        .BeginBarrierList()
            .AddBufferBarrier(s_commonOpaqueMeshDrawCmdBuffer, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
            .AddBufferBarrier(s_commonAKillMeshDrawCmdBuffer, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
            .AddBufferBarrier(s_commonTranspMeshDrawCmdBuffer, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
        .Push();

    static constexpr VkDeviceSize COUNTER_OFFSET = 0; 
    static constexpr VkDeviceSize COUNTER_SIZE = sizeof(glm::uint);

    cmdBuffer.CmdFillBuffer(s_commonOpaqueMeshDrawCmdBuffer, 0, COUNTER_OFFSET, COUNTER_SIZE);
    cmdBuffer.CmdFillBuffer(s_commonAKillMeshDrawCmdBuffer,  0, COUNTER_OFFSET, COUNTER_SIZE);
    cmdBuffer.CmdFillBuffer(s_commonTranspMeshDrawCmdBuffer, 0, COUNTER_OFFSET, COUNTER_SIZE);
}


static void GeomCullingPass(vkn::CmdBuffer& cmdBuffer, PassID pass)
{
    CORE_ASSERT(pass == PassID::GEOM_CULLING_OCCLUDERS || pass == PassID::GEOM_CULLING_OCCLUSION);

    ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::Blue3, "Geom_Culling_Pass_%u", (uint32_t)pass);

    // if (pass == PassID::GEOM_CULLING_OCCLUDERS) {
        GeomCullingClearCmdBuffers(cmdBuffer);
    // }

    vkn::BarrierList& barriers = cmdBuffer.BeginBarrierList();

    if (pass == PassID::GEOM_CULLING_OCCLUSION) {
        barriers.AddTextureBarrier(s_HZB, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    static auto GetGeomVisFlagsBufferAccessMask = [](PassID passID)
    {
        return passID == PassID::GEOM_CULLING_OCCLUSION ? VK_ACCESS_2_SHADER_READ_BIT : VK_ACCESS_2_SHADER_WRITE_BIT;
    };

    barriers
        .AddBufferBarrier(s_commonOpaqueMeshDrawCmdBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
        .AddBufferBarrier(s_commonAKillMeshDrawCmdBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
        .AddBufferBarrier(s_commonTranspMeshDrawCmdBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
        .AddBufferBarrier(s_commonCulledOpaqueInstInfoIDsBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
        .AddBufferBarrier(s_commonCulledAKillInstInfoIDsBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
        .AddBufferBarrier(s_commonCulledTranspInstInfoIDsBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
        .AddBufferBarrier(s_commonGeomVisFlagsBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, GetGeomVisFlagsBufferAccessMask(pass))
    .Push();

    vkn::PSO& pso = s_PSOs[(size_t)pass];

    cmdBuffer.CmdBindPSO(pso);
    
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)pass, .shaderSetIdx = DESC_SET_PER_DRAW });

    MESH_CULLING_PER_DRAW_DATA pushConsts = {};
    pushConsts.INST_COUNT = s_cpuInstData.size();
    pushConsts.HZB_MIPS_COUNT = s_HZB.GetMipCount();
    pushConsts.VIS_CONTRIBUTION_FALLOFF = s_commonVisContributionFalloff;

    cmdBuffer.CmdPushConstants(pso, VK_SHADER_STAGE_COMPUTE_BIT, pushConsts);

    cmdBuffer.CmdDispatch(ceil(s_cpuInstData.size() / (float)GEOM_CULLING_CS_GPOUP_SIZE), 1, 1);
}


static void GeomCullingPassOccluders(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Geom_Culling_Pass_Occluders", eng::ProfileColor::Blue3);
    GeomCullingPass(cmdBuffer, PassID::GEOM_CULLING_OCCLUDERS);
}


static void GeomCullingPassOcclusion(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Geom_Culling_Pass_Occlusion", eng::ProfileColor::Blue3);
    GeomCullingPass(cmdBuffer, PassID::GEOM_CULLING_OCCLUSION);
}


void RenderPass_Depth(vkn::CmdBuffer& cmdBuffer, bool isAKillPass)
{
    vkn::BarrierList& barrierList = cmdBuffer.BeginBarrierList();

    barrierList.AddTextureBarrier(s_depthRT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

    if (s_useMeshIndirectDraw) {
        vkn::Buffer& drawCmdBuffer = isAKillPass ? s_commonAKillMeshDrawCmdBuffer : s_commonOpaqueMeshDrawCmdBuffer;

        barrierList.AddBufferBarrier(drawCmdBuffer, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
    }

    barrierList.AddBufferBarrier(
        isAKillPass ? s_commonCulledAKillInstInfoIDsBuffer : s_commonCulledOpaqueInstInfoIDsBuffer, 
        s_useMeshIndirectDraw ? VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT : VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, 
        s_useMeshIndirectDraw ? VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT : VK_ACCESS_2_SHADER_READ_BIT
    );
    
    barrierList.Push();

    const VkExtent2D extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = extent;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView   = s_depthRTView.Get();
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
        cmdBuffer.CmdSetViewport(0.f, 0.f, extent.width, extent.height);
        cmdBuffer.CmdSetScissor(0, 0, extent.width, extent.height);

        vkn::PSO& pso = s_PSOs[(size_t)PassID::DEPTH];

        cmdBuffer.CmdBindPSO(pso);
        
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::DEPTH, .shaderSetIdx = DESC_SET_PER_DRAW });

        cmdBuffer.CmdBindIndexBuffer(s_indexBuffer, 0, GetVkIndexType());

        ZPASS_PER_DRAW_DATA pushConsts = {};
        pushConsts.IS_AKILL_PASS = isAKillPass;

        if (s_useMeshIndirectDraw) {
            cmdBuffer.CmdPushConstants(pso, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, pushConsts);

            if (isAKillPass) {
                cmdBuffer.CmdDrawIndexedIndirect(s_commonAKillMeshDrawCmdBuffer, sizeof(glm::uint), s_commonAKillMeshDrawCmdBuffer, 0, 
                    MAX_INDIRECT_DRAW_CMD_COUNT, sizeof(COMMON_CMD_DRAW_INDEXED_INDIRECT));
            } else {
                cmdBuffer.CmdDrawIndexedIndirect(s_commonOpaqueMeshDrawCmdBuffer, sizeof(glm::uint), s_commonOpaqueMeshDrawCmdBuffer, 0, 
                    MAX_INDIRECT_DRAW_CMD_COUNT, sizeof(COMMON_CMD_DRAW_INDEXED_INDIRECT));
            }
        } else {
            ENG_PROFILE_SCOPED_MARKER_C("Depth_CPU_Frustum_Culling", eng::ProfileColor::Purple1);

            for (uint32_t i = 0; i < s_cpuInstData.size(); ++i) {
                const COMMON_INST_DATA& instInfo = s_cpuInstData[i];
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

                cmdBuffer.CmdPushConstants(pso, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, pushConsts);

                const COMMON_MESH_DATA& mesh = s_cpuMeshData[s_cpuInstData[i].MESH_IDX];
                cmdBuffer.CmdDrawIndexed(mesh.INDEX_COUNT, 1, mesh.FIRST_INDEX, mesh.FIRST_VERTEX, i);
            }
        }
    cmdBuffer.CmdEndRendering();

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_depthRT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, 
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_ASPECT_DEPTH_BIT)
        .Push();
}


void DepthPass(vkn::CmdBuffer& cmdBuffer)
{
    if (!s_useDepthPass) {
        return;
    }

    ENG_PROFILE_SCOPED_MARKER_C("Depth_Pass", eng::ProfileColor::Grey51);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Depth_Pass", eng::ProfileColor::Grey51);

    {
        ENG_PROFILE_SCOPED_MARKER_C("Depth_Pass_Opaque", eng::ProfileColor::Grey51);
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Depth_Pass_Opaque", eng::ProfileColor::Grey51);
        RenderPass_Depth(cmdBuffer, false);
    }
    {
        ENG_PROFILE_SCOPED_MARKER_C("Depth_Pass_AKill", eng::ProfileColor::Grey51);
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Depth_Pass_AKill", eng::ProfileColor::Grey51);
        RenderPass_Depth(cmdBuffer, true);
    }
}


void RenderPass_GBuffer(vkn::CmdBuffer& cmdBuffer, bool isAKillPass)
{
    vkn::BarrierList& barrierList = cmdBuffer.BeginBarrierList();
 
    for (vkn::Texture& colorRT : s_gbufferRTs) {
        barrierList.AddTextureBarrier(colorRT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    if (s_useDepthPass) {
        barrierList.AddTextureBarrier(s_depthRT, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, 
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
    } else {
        barrierList.AddTextureBarrier(s_depthRT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    if (s_useMeshIndirectDraw) {
        vkn::Buffer& drawCmdBuffer = isAKillPass ? s_commonAKillMeshDrawCmdBuffer : s_commonOpaqueMeshDrawCmdBuffer;

        barrierList.AddBufferBarrier(drawCmdBuffer, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
    }

    barrierList.AddBufferBarrier(
        isAKillPass ? s_commonCulledAKillInstInfoIDsBuffer : s_commonCulledOpaqueInstInfoIDsBuffer, 
        s_useMeshIndirectDraw ? VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT : VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, 
        s_useMeshIndirectDraw ? VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT : VK_ACCESS_2_SHADER_READ_BIT
    );

    barrierList.Push();    

    const VkExtent2D extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = extent;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = s_depthRTView.Get();
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
        cmdBuffer.CmdSetViewport(0.f, 0.f, extent.width, extent.height);
        cmdBuffer.CmdSetScissor(0, 0, extent.width, extent.height);

        vkn::PSO& pso = s_PSOs[(size_t)PassID::GBUFFER];

        cmdBuffer.CmdBindPSO(pso);
        
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::GBUFFER, .shaderSetIdx = DESC_SET_PER_DRAW });

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

        GBUFFER_PER_DRAW_DATA pushConsts = {};
        pushConsts.IS_AKILL_PASS = isAKillPass;

        if (s_useMeshIndirectDraw) {
            cmdBuffer.CmdPushConstants(pso, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, pushConsts);
            
            if (isAKillPass) {
                cmdBuffer.CmdDrawIndexedIndirect(s_commonAKillMeshDrawCmdBuffer, sizeof(glm::uint), s_commonAKillMeshDrawCmdBuffer, 0, 
                    MAX_INDIRECT_DRAW_CMD_COUNT, sizeof(COMMON_CMD_DRAW_INDEXED_INDIRECT));
            } else {
                cmdBuffer.CmdDrawIndexedIndirect(s_commonOpaqueMeshDrawCmdBuffer, sizeof(glm::uint), s_commonOpaqueMeshDrawCmdBuffer, 0, 
                    MAX_INDIRECT_DRAW_CMD_COUNT, sizeof(COMMON_CMD_DRAW_INDEXED_INDIRECT));
            }
        } else {
            ENG_PROFILE_SCOPED_MARKER_C("GBuffer_CPU_Frustum_Culling", eng::ProfileColor::Purple1);

        #ifdef ENG_BUILD_DEBUG
            if (isAKillPass) {
                s_dbgDrawnAkillMeshCount = 0;
            } else {
                s_dbgDrawnOpaqueMeshCount = 0;
            }
        #endif

            for (uint32_t i = 0; i < s_cpuInstData.size(); ++i) {
                const COMMON_INST_DATA& instInfo = s_cpuInstData[i];
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

                cmdBuffer.CmdPushConstants(pso, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, pushConsts);
                
                const COMMON_MESH_DATA& mesh = s_cpuMeshData[s_cpuInstData[i].MESH_IDX];
                cmdBuffer.CmdDrawIndexed(mesh.INDEX_COUNT, 1, mesh.FIRST_INDEX, mesh.FIRST_VERTEX, i);
            }
        }
    cmdBuffer.CmdEndRendering();
}


void GBufferRenderPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_SCOPED_MARKER_C("GBuffer_Pass", eng::ProfileColor::ForestGreen);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "GBuffer_Pass", eng::ProfileColor::ForestGreen);

    {
        ENG_PROFILE_SCOPED_MARKER_C("GBuffer_Pass_Opaque", eng::ProfileColor::ForestGreen);
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "GBuffer_Pass_Opaque", eng::ProfileColor::ForestGreen);
        RenderPass_GBuffer(cmdBuffer, false);
    }
    {
        ENG_PROFILE_SCOPED_MARKER_C("GBuffer_Pass_AKill", eng::ProfileColor::ForestGreen);
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "GBuffer_Pass_AKill", eng::ProfileColor::ForestGreen);
        RenderPass_GBuffer(cmdBuffer, true);
    }
}


void DeferredLightingPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_SCOPED_MARKER_C("Deferred_Lighting_Pass", eng::ProfileColor::Yellow);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Deferred_Lighting_Pass", eng::ProfileColor::Yellow);

    vkn::BarrierList& barrierList = cmdBuffer.BeginBarrierList();

    for (vkn::Texture& gbufferRT : s_gbufferRTs) {
        barrierList.AddTextureBarrier(gbufferRT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    barrierList.AddTextureBarrier(s_colorRT16F, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    barrierList.AddTextureBarrier(s_depthRT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

    barrierList.Push();
    
    const VkExtent2D extent = VkExtent2D { s_colorRT16F.GetSizeX(), s_colorRT16F.GetSizeY() };

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = extent;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;

    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = s_colorRTView16F.Get();
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
        cmdBuffer.CmdSetViewport(0.f, 0.f, extent.width, extent.height);
        cmdBuffer.CmdSetScissor(0, 0, extent.width, extent.height);

        vkn::PSO& pso = s_PSOs[(size_t)PassID::DEFERRED_LIGHTING];

        cmdBuffer.CmdBindPSO(pso);
        
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::DEFERRED_LIGHTING, .shaderSetIdx = DESC_SET_PER_DRAW });

        cmdBuffer.CmdDraw(6, 1, 0, 0);        
    cmdBuffer.CmdEndRendering();
}


void SkyboxPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_SCOPED_MARKER_C("Skybox_Pass", eng::ProfileColor::Aquamarine);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Skybox_Pass", eng::ProfileColor::Aquamarine);

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_colorRT16F, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
            .AddTextureBarrier(s_depthRT, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, 
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_ASPECT_DEPTH_BIT)
        .Push();

    const VkExtent2D extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = extent;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;

    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = s_colorRTView16F.Get(),
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = s_depthRTView.Get();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    renderingInfo.pDepthAttachment = &depthAttachment;

    cmdBuffer.CmdBeginRendering(renderingInfo);
        cmdBuffer.CmdSetViewport(0.f, 0.f, extent.width, extent.height);
        cmdBuffer.CmdSetScissor(0, 0, extent.width, extent.height);

        vkn::PSO& pso = s_PSOs[(size_t)PassID::SKYBOX];

        cmdBuffer.CmdBindPSO(pso);
        
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::SKYBOX, .shaderSetIdx = DESC_SET_PER_DRAW });

        cmdBuffer.CmdDraw(36, 1, 0, 0);        
    cmdBuffer.CmdEndRendering();
}


void PostProcessingPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_SCOPED_MARKER_C("Post_Processing_Pass", eng::ProfileColor::RebeccaPurple);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Post_Processing_Pass", eng::ProfileColor::RebeccaPurple);

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_colorRT16F, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, 
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
            .AddTextureBarrier(s_colorRT8U, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
        .Push();

    const VkExtent2D extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = extent;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;

    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = s_colorRTView8U.Get();
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
        cmdBuffer.CmdSetViewport(0.f, 0.f, extent.width, extent.height);
        cmdBuffer.CmdSetScissor(0, 0, extent.width, extent.height);

        vkn::PSO& pso = s_PSOs[(size_t)PassID::POST_PROCESSING];

        cmdBuffer.CmdBindPSO(pso);
        
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::POST_PROCESSING, .shaderSetIdx = DESC_SET_PER_DRAW });

        cmdBuffer.CmdDraw(6, 1, 0, 0);        
    cmdBuffer.CmdEndRendering();
}


#ifdef ENG_DEBUG_DRAW_ENABLED
static void DbgDrawPass(vkn::CmdBuffer& cmdBuffer)
{
    const uint32_t lineInstCount = s_dbgLineDataCPU.size();
    const uint32_t triInstCount = s_dbgTriangleDataCPU.size();

    if (lineInstCount == 0 && triInstCount == 0) {
        return;
    }

    ENG_PROFILE_SCOPED_MARKER_C("Dbg_Draw_Render_Pass", eng::ProfileColor::Red);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Dbg_Draw_Render_Pass", eng::ProfileColor::Red);

    if (lineInstCount > 0) {
        void* pLineData = s_dbgLineDataGPU.Map();
        memcpy(pLineData, s_dbgLineDataCPU.data(), lineInstCount * sizeof(DBG_LINE_DATA));
        s_dbgLineDataGPU.Unmap();

        void* pLineVertData = s_dbgLineVertexDataGPU.Map();
        memcpy(pLineVertData, s_dbgLineVertexDataCPU.data(), s_dbgLineVertexDataCPU.size() * sizeof(s_dbgLineVertexDataCPU[0]));
        s_dbgLineVertexDataGPU.Unmap();
    }

    if (triInstCount > 0) {
        void* pTriData = s_dbgTriangleDataGPU.Map();
        memcpy(pTriData, s_dbgTriangleDataCPU.data(), triInstCount * sizeof(DBG_TRIANGLE_DATA));
        s_dbgTriangleDataGPU.Unmap();

        void* pTriVertData = s_dbgTriangleVertexDataGPU.Map();
        memcpy(pTriVertData, s_dbgTriangleVertexDataCPU.data(), s_dbgTriangleVertexDataCPU.size() * sizeof(s_dbgTriangleVertexDataCPU[0]));
        s_dbgTriangleVertexDataGPU.Unmap();
    }

    vkn::BarrierList& barriers = cmdBuffer.BeginBarrierList();

    barriers.AddTextureBarrier(s_colorRT8U, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    barriers.AddTextureBarrier(s_depthRT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, 
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);  

    if (lineInstCount > 0) {
        barriers.AddBufferBarrier(s_dbgLineDataGPU, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_READ_BIT);
        barriers.AddBufferBarrier(s_dbgLineVertexDataGPU, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, 
            VK_ACCESS_2_SHADER_READ_BIT);
    }
    
    if (triInstCount > 0) {
        barriers.AddBufferBarrier(s_dbgTriangleDataGPU, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_READ_BIT);
        barriers.AddBufferBarrier(s_dbgTriangleVertexDataGPU, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, 
            VK_ACCESS_2_SHADER_READ_BIT);
    }
        
    barriers.Push();

    const VkExtent2D extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = extent;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;

    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = s_colorRTView8U.Get();
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = s_depthRTView.Get();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    renderingInfo.pDepthAttachment = &depthAttachment;

    cmdBuffer.CmdBeginRendering(renderingInfo);
        cmdBuffer.CmdSetViewport(0.f, 0.f, extent.width, extent.height);
        cmdBuffer.CmdSetScissor(0, 0, extent.width, extent.height);

        if (lineInstCount > 0) {
            ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Dbg_Draw_Render_Pass_Lines", eng::ProfileColor::Red2);
    
            vkn::PSO& pso = s_PSOs[(size_t)PassID::DBG_DRAW_LINES];

            cmdBuffer.CmdBindPSO(pso);
            
            cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
            cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::DBG_DRAW_LINES, .shaderSetIdx = DESC_SET_PER_DRAW });
    
            cmdBuffer.CmdDraw(DBG_LINE_VERTEX_COUNT, lineInstCount, 0, 0);     
        }

        if (triInstCount > 0) {
            ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Dbg_Draw_Render_Pass_Triangles", eng::ProfileColor::Red2);

            vkn::PSO& pso = s_PSOs[(size_t)PassID::DBG_DRAW_TRIANGLES];

            cmdBuffer.CmdBindPSO(pso);
            
            cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
            cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::DBG_DRAW_TRIANGLES, .shaderSetIdx = DESC_SET_PER_DRAW });
    
            cmdBuffer.CmdDraw(DBG_TRIANGLE_VERTEX_COUNT, triInstCount, 0, 0);     
        }
    cmdBuffer.CmdEndRendering();
}
#endif


#ifdef ENG_DEBUG_UI_ENABLED
static void DbgUIPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_SCOPED_MARKER_C("Dbg_UI_Render_Pass", eng::ProfileColor::Red);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Dbg_UI_Render_Pass", eng::ProfileColor::Red);

    s_dbgUI.BeginFrame(s_frameTime);

    DbgUI::FillData();

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_colorRT8U, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
        .Push();

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;

    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = s_colorRTView8U.Get();
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    cmdBuffer.CmdBeginRendering(renderingInfo);
        s_dbgUI.Render(cmdBuffer);
    cmdBuffer.CmdEndRendering();

    // ImGui uses descriptor sets so we need to rebind descriptor buffer
    cmdBuffer.CmdBindDescriptorBuffer(s_descriptorBuffer);
}
#endif


void ResolveToBackbufferPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_SCOPED_MARKER_C("Resolve_To_Backbuffer_Pass", eng::ProfileColor::DimGrey);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Resolve_To_Backbuffer_Pass", eng::ProfileColor::DimGrey);

    vkn::SCTexture& scTexture = s_vkSwapchain.GetTexture(s_nextImageIdx);
    vkn::SCTextureView& scTextureView = s_vkSwapchain.GetTextureView(s_nextImageIdx);

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_colorRT8U, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
            .AddTextureBarrier(scTexture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
        .Push();

    const VkExtent2D extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = extent;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;

    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = scTextureView.Get();
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
        cmdBuffer.CmdSetViewport(0.f, 0.f, extent.width, extent.height);
        cmdBuffer.CmdSetScissor(0, 0, extent.width, extent.height);

        vkn::PSO& pso = s_PSOs[(size_t)PassID::BACKBUFFER];

        cmdBuffer.CmdBindPSO(pso);
        
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .bufferSetIdx = (uint32_t)PassID::BACKBUFFER, .shaderSetIdx = DESC_SET_PER_DRAW });

        cmdBuffer.CmdDraw(6, 1, 0, 0);        
    cmdBuffer.CmdEndRendering();

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(scTexture, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 
                VK_ACCESS_2_NONE, VK_IMAGE_ASPECT_COLOR_BIT)
        .Push();
}


static void RenderScene()
{
    if (s_renderFinishedFence.GetStatus() == VK_NOT_READY) {
        return;
    }

    ENG_PROFILE_SCOPED_MARKER_C("Render_Scene", eng::ProfileColor::DimGrey);

    UpdateGPUCommonConstBuffer();

    const VkResult acquireResult = vkAcquireNextImageKHR(s_vkDevice.Get(), s_vkSwapchain.Get(), 10'000'000'000, s_presentFinishedSemaphore.Get(), VK_NULL_HANDLE, &s_nextImageIdx);
    
    if (acquireResult != VK_SUBOPTIMAL_KHR && acquireResult != VK_ERROR_OUT_OF_DATE_KHR) {
        VK_CHECK(acquireResult);
    } else {
        s_swapchainRecreateRequired = !s_pWnd->IsMinimized() && s_pWnd->GetWidth() != 0 &&  s_pWnd->GetHeight() != 0;
        return;
    }

    vkn::Semaphore& renderingFinishedSemaphore = s_renderFinishedSemaphores[s_nextImageIdx];
    vkn::CmdBuffer& cmdBuffer = *s_pRenderCmdBuffer;

    cmdBuffer.Reset();

    cmdBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    {
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Render_Scene_GPU", eng::ProfileColor::Grey10);

        cmdBuffer.CmdBindDescriptorBuffer(s_descriptorBuffer);

        // GeomCullingPassOccluders(cmdBuffer);
        // OccludersDepthPass(cmdBuffer);

        GeomCullingPassOcclusion(cmdBuffer);
        DepthPass(cmdBuffer);
        HZBGeneratePass(cmdBuffer);

        // GeomCullingPassOcclusion(cmdBuffer);
        // DepthPass(cmdBuffer);

        GBufferRenderPass(cmdBuffer);
        DeferredLightingPass(cmdBuffer);

        SkyboxPass(cmdBuffer);

        PostProcessingPass(cmdBuffer);

    #ifdef ENG_DEBUG_DRAW_ENABLED
        DbgDrawPass(cmdBuffer);
    #endif

    #ifdef ENG_DEBUG_UI_ENABLED
        DbgUIPass(cmdBuffer);
    #endif

        ResolveToBackbufferPass(cmdBuffer);

        ENG_PROFILE_GPU_COLLECT_STATS(cmdBuffer);
    }
    cmdBuffer.End();

    s_renderFinishedFence.Reset();

    vkn::QueueSyncData waitData = { &s_presentFinishedSemaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT };
    vkn::QueueSyncData signalData = { &renderingFinishedSemaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT };

    s_vkDevice.GetQueue().Submit(cmdBuffer, &s_renderFinishedFence, &waitData, &signalData);

    PresentImage(s_nextImageIdx);
}


static void CameraProcessWndEvent(eng::Camera& camera, const eng::WndEvent& event)
{
    static bool firstEvent = true;

    if (event.Is<eng::WndKeyEvent>()) {
        const eng::WndKeyEvent& keyEvent = event.Get<eng::WndKeyEvent>();

        if (keyEvent.IsPressed()) {
            const float finalSpeed = CAMERA_SPEED * s_frameTime;

            if (keyEvent.key == eng::WndKey::KEY_W) { 
                s_cameraVel.z = -finalSpeed;
            }
            if (keyEvent.key == eng::WndKey::KEY_S) {
                s_cameraVel.z = finalSpeed;
            }
            if (keyEvent.key == eng::WndKey::KEY_A) {
                s_cameraVel.x = -finalSpeed;
            }
            if (keyEvent.key == eng::WndKey::KEY_D) {
                s_cameraVel.x = finalSpeed;
            }
            if (keyEvent.key == eng::WndKey::KEY_E) {
                s_cameraVel.y = finalSpeed;
            }
            if (keyEvent.key == eng::WndKey::KEY_Q) {
                s_cameraVel.y = -finalSpeed;
            }
            if (keyEvent.key == eng::WndKey::KEY_F5) {
                firstEvent = true;
            }
        }

        if (keyEvent.IsReleased()) {
            if (keyEvent.key == eng::WndKey::KEY_W) {
                s_cameraVel.z = 0;
            }
            if (keyEvent.key == eng::WndKey::KEY_S) {
                s_cameraVel.z = 0;
            }
            if (keyEvent.key == eng::WndKey::KEY_A) {
                s_cameraVel.x = 0;
            }
            if (keyEvent.key == eng::WndKey::KEY_D) {
                s_cameraVel.x = 0;
            }
            if (keyEvent.key == eng::WndKey::KEY_E) {
                s_cameraVel.y = 0;
            }
            if (keyEvent.key == eng::WndKey::KEY_Q) {
                s_cameraVel.y = 0;
            }
        }
    } else if (event.Is<eng::WndCursorEvent>()) {
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
    } else if (event.Is<eng::WndResizeEvent>()) {
        const eng::WndResizeEvent& resizeEvent = event.Get<eng::WndResizeEvent>();

        if (!resizeEvent.IsMinimized() && resizeEvent.height != 0) {
            camera.SetAspectRatio((float)resizeEvent.width / (float)resizeEvent.height);
        }
    }
}


void AppProcessWndEvent(const eng::WndEvent& event)
{
    s_dbgUI.ProcessEvent(event);

    if (event.Is<eng::WndKeyEvent>()) {
        const eng::WndKeyEvent& keyEvent = event.Get<eng::WndKeyEvent>();

        if (keyEvent.key == eng::WndKey::KEY_F5 && keyEvent.IsPressed()) {
            s_flyCameraMode = !s_flyCameraMode;
            s_pWnd->SetCursorRelativeMode(s_flyCameraMode);
        } else if (keyEvent.key == eng::WndKey::KEY_F6 && keyEvent.IsPressed()) {
            s_cullingTestMode = !s_cullingTestMode;

            if (s_cullingTestMode) {
                s_fixedCamCullViewProjMatr = s_camera.GetViewProjMatrix();
                s_fixedCamCullInvViewProjMatr = s_camera.GetInvViewProjMatrix();
                s_fixedCamCullFrustum = s_camera.GetFrustum();
            }
        }
    }

    if (s_flyCameraMode) {
        CameraProcessWndEvent(s_camera, event);
    }

    if (event.Is<eng::WndResizeEvent>()) {
        const eng::WndResizeEvent& e = event.Get<eng::WndResizeEvent>();

        if (!e.IsMinimized() && e.width != 0 && e.height != 0) {
            s_swapchainRecreateRequired = true;
        }
    }
}


void ProcessFrame()
{
    ENG_PROFILE_BEGIN_FRAME("Frame");

    static eng::Timer timer;
    timer.End().GetDuration<float, std::milli>(s_frameTime).Reset();

    s_pWnd->SetTitle("%s | Build Type: %s | CPU: %.3f ms (%.1f FPS)", APP_NAME, APP_BUILD_TYPE_STR, s_frameTime, 1000.f / s_frameTime);

    s_skipRender = false;

    s_pWnd->PullEvents();

    eng::WndEvent event;
    while(s_pWnd->PopEvent(event)) {
        AppProcessWndEvent(event);
    }

    UpdateScene();

    s_skipRender = s_skipRender || s_pWnd->IsMinimized() || s_pWnd->GetWidth() == 0 || s_pWnd->GetHeight() == 0;

    if (!s_skipRender) {
        RenderScene();
    }

    ++s_frameNumber;

    ENG_PROFILE_END_FRAME("Frame");
}


int main(int argc, char* argv[])
{
    // LoadScene(argc > 1 ? argv[1] : "../assets/Sponza/Sponza.gltf");
    // LoadScene(argc > 1 ? argv[1] : "../assets/LightSponza/Sponza.gltf");
    // LoadScene(argc > 1 ? argv[1] : "../assets/TestPBR/TestPBR.gltf");
    LoadScene(argc > 1 ? argv[1] : "../assets/GPUOcclusionTest/Occlusion.gltf");

    eng::WindowInitInfo wndInitInfo = {};
    wndInitInfo.pTitle = APP_NAME;
    wndInitInfo.width = 1280;
    wndInitInfo.height = 720;
    wndInitInfo.isVisible = false;

    s_pWnd = std::make_unique<eng::Win32Window>(wndInitInfo);
    ENG_ASSERT(s_pWnd && s_pWnd->IsCreated());

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
    vkAllocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    s_vkAllocator.Create(vkAllocatorCreateInfo);
    CORE_ASSERT(s_vkAllocator.IsCreated());

    CreateVkSwapchain();

    vkn::CmdPoolCreateInfo cmdPoolCreateInfo = {};
    cmdPoolCreateInfo.pDevice = &s_vkDevice;
    cmdPoolCreateInfo.queueFamilyIndex = s_vkDevice.GetQueue().GetFamilyIndex();
    cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdPoolCreateInfo.size = 2;
    
    s_commonCmdPool.Create(cmdPoolCreateInfo).SetDebugName("COMMON_CMD_POOL");
    
    s_pImmediateSubmitCmdBuffer = s_commonCmdPool.AllocCmdBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    s_pImmediateSubmitCmdBuffer->SetDebugName("IMMEDIATE_CMD_BUFFER");

    s_immediateSubmitFinishedFence.Create(&s_vkDevice);

    CreateCommonStagingBuffers();

    CreateDynamicRenderTargets();

    CreateCommonSamplers();
    CreateCommonConstBuffer();
    CreateCullingResources();
    CreateCommonDbgTextures();
    CreateDbgDrawResources();
    CreateDescriptorSets();
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

    s_dbgUI.Create(*s_pWnd, s_vkDevice, s_colorRT8U.GetFormat());

    const size_t swapchainImageCount = s_vkSwapchain.GetTextureCount();

    s_renderFinishedSemaphores.resize(swapchainImageCount);
    for (size_t i = 0; i < swapchainImageCount; ++i) {
        s_renderFinishedSemaphores[i].Create(&s_vkDevice).SetDebugName("RND_FINISH_SEMAPHORE_%zu", i);
    }
    s_presentFinishedSemaphore.Create(&s_vkDevice).SetDebugName("PRESENT_FINISH_SEMAPHORE");

    s_renderFinishedFence.Create(&s_vkDevice).SetDebugName("RND_FINISH_FENCE");
    
    s_pRenderCmdBuffer = s_commonCmdPool.AllocCmdBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    s_pRenderCmdBuffer->SetDebugName("RND_CMD_BUFFER");

    UploadGPUResources();
    CreateIBLResources();

    WriteDescriptorSets();

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        cmdBuffer.CmdBindDescriptorBuffer(s_descriptorBuffer);

        PrecomputeIBLIrradianceMap(cmdBuffer);
        PrecomputeIBLPrefilteredEnvMap(cmdBuffer);
        PrecomputeIBLBRDFIntergrationLUT(cmdBuffer);
    });

    s_cpuTexturesData.clear();

    s_camera.SetPosition(glm::float3(0.f, 0.f, 6.f));
    s_camera.SetRotation(glm::quatLookAt(-M3D_AXIS_Z, M3D_AXIS_Y));
    s_camera.SetPerspProjection(glm::radians(90.f), (float)s_pWnd->GetWidth() / s_pWnd->GetHeight(), 0.01f, 10'000.f);

    s_pWnd->SetVisible(true);

    while(!s_pWnd->IsClosed()) {
        ProcessFrame();
    }

    s_vkDevice.WaitIdle();

    return 0;
}