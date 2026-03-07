#include "core/platform/window/window.h"

#ifdef ENG_OS_WINDOWS
    #include "core/platform/native/win32/window/win32_window.h"
#else
    #error Unsupported OS type!
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
    glm::uint PAD;
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


static constexpr glm::uint COMMON_INDIRECT_DRAW_CMD_SIZE_UINT = 5;
static_assert(sizeof(COMMON_INDIRECT_DRAW_CMD) == COMMON_INDIRECT_DRAW_CMD_SIZE_UINT * sizeof(glm::uint));


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

    glm::uint COMMON_FLAGS;
    glm::uint COMMON_DBG_FLAGS;
    glm::uint COMMON_DBG_VIS_FLAGS;
    glm::uint PAD0;

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
static constexpr size_t MESH_CULL_OPAQUE_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT = 3;
static constexpr size_t MESH_CULL_AKILL_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT = 4;
static constexpr size_t MESH_CULL_TRANSP_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT = 5;

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


static constexpr uint32_t COMMON_BINDLESS_TEXTURES_COUNT = 128;

static constexpr uint32_t MAX_INDIRECT_DRAW_CMD_COUNT = 1024;

static constexpr size_t GBUFFER_RT_COUNT = 4;
static constexpr size_t CUBEMAP_FACE_COUNT = 6;

static constexpr size_t STAGING_BUFFER_SIZE  = 96 * 1024 * 1024; // 96 MB
static constexpr size_t STAGING_BUFFER_COUNT = 2;


static constexpr glm::uint  COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT = 10;
static constexpr float      COMMON_PREFILTERED_ENV_MAP_MIP_ROUGHNESS_DELTA = 1.f / (COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1);

static constexpr glm::uvec2 COMMON_IRRADIANCE_MAP_SIZE = glm::uvec2(32);
static constexpr glm::uvec2 COMMON_PREFILTERED_ENV_MAP_SIZE = glm::uvec2(glm::uint(1) << (COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1));
static constexpr glm::uvec2 COMMON_BRDF_INTEGRATION_LUT_SIZE = glm::uvec2(512);


static constexpr const char* APP_NAME = "Vulkan Demo";

#if defined(ENG_BUILD_DEBUG)
    constexpr const char* APP_BUILD_TYPE_STR = "DEBUG";
#elif defined(ENG_BUILD_PROFILE)
    constexpr const char* APP_BUILD_TYPE_STR = "PROFILE";
#else
    constexpr const char* APP_BUILD_TYPE_STR = "RELEASE";
#endif  

static constexpr bool VSYNC_ENABLED = false;

static constexpr float CAMERA_SPEED = 0.0025f;


enum class PassID
{
    COMMON,
    IRRADIANCE_MAP_GEN,
    BRDF_LUT_GEN,
    PREFILT_ENV_MAP_GEN,
    MESH_CULLING,
    ZPASS,
    GBUFFER,
    DEFERRED_LIGHTING,
    SKYBOX,
    POST_PROCESSING,
    BACKBUFFER,
    COUNT,
};


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
static vkn::Buffer s_commonInstDataBuffer;

static vkn::Buffer s_commonOpaqueMeshDrawCmdBuffer;
static vkn::Buffer s_commonCulledOpaqueInstInfoIDsBuffer;

static vkn::Buffer s_commonAKillMeshDrawCmdBuffer;
static vkn::Buffer s_commonCulledAKillInstInfoIDsBuffer;

static vkn::Buffer s_commonTranspMeshDrawCmdBuffer;
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
static std::array<vkn::TextureView, COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT * CUBEMAP_FACE_COUNT> s_prefilteredEnvMapTextureViewRWs;

static vkn::Texture     s_brdfLUTTexture;
static vkn::TextureView s_brdfLUTTextureView;
static vkn::TextureView s_brdfLUTTextureViewRW;

static std::array<vkn::Texture, GBUFFER_RT_COUNT>     s_gbufferRTs;
static std::array<vkn::TextureView, GBUFFER_RT_COUNT> s_gbufferRTViews;

static vkn::Texture     s_commonDepthRT;
static vkn::TextureView s_commonDepthRTView;

static vkn::Texture     s_colorRT8U;
static vkn::TextureView s_colorRTView8U;

static vkn::Texture     s_colorRT16F;
static vkn::TextureView s_colorRTView16F;

static vkn::ComputePSOBuilder s_computePSOBuilder;
static vkn::GraphicsPSOBuilder s_graphicsPSOBuilder;
static std::vector<uint8_t> s_shaderCodeBuffer;

static eng::DbgUI s_dbgUI;

static eng::Camera s_camera;
static glm::float3 s_cameraVel = M3D_ZEROF3;

static uint32_t s_dbgOutputRTIdx = 0;
static uint32_t s_nextImageIdx = 0;

static size_t s_frameNumber = 0;
static float s_frameTime = M3D_EPS;
static bool s_swapchainRecreateRequired = false;
static bool s_flyCameraMode = false;

static bool s_skipRender = false;

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

            ImGui::Text("Vertex Buffer Size: %.3f MB", s_cpuVertexBuffer.size() * sizeof(Vertex) / 1024.f / 1024.f);
            ImGui::Text("Index Buffer Size: %.3f MB", s_cpuIndexBuffer.size() * sizeof(IndexType) / 1024.f / 1024.f);

            ImGui::NewLine();
            ImGui::SeparatorText("Camera Info");
            ImGui::Text("Fly Camera Mode (F5):");
            ImGui::SameLine(); 
            ImGui::TextColored(ImVec4(!s_flyCameraMode, s_flyCameraMode, 0.f, 1.f), s_flyCameraMode ? "ON" : "OFF");

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
            .AddTextureBarrier(s_colorRT16F, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_ASPECT_COLOR_BIT);
        cmdBuffer.CmdPushBarrierList();
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

    s_colorRTView16F.Destroy();
    s_colorRT16F.Destroy();

    s_colorRTView8U.Destroy();
    s_colorRT8U.Destroy();
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
                VK_IMAGE_ASPECT_COLOR_BIT, mip, 1, layerIdx, 1);
        cmdBuffer.CmdPushBarrierList();

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
    cmdBuffer.BeginBarrierList().AddTextureBarrier(texture, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT, 
        VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, loadData.GetMipsCount() - 1, 1, layerIdx, 1);
    cmdBuffer.CmdPushBarrierList();
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
            cmdBuffer.BeginBarrierList().AddTextureBarrier(s_skyboxTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT, 
                VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
            cmdBuffer.CmdPushBarrierList();

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

        cmdBuffer.BeginBarrierList().AddTextureBarrier(s_skyboxTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        cmdBuffer.CmdPushBarrierList();
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


static bool LoadShaderSpirVCode(const fs::path& path, std::vector<uint8_t>& buffer)
{
    const std::string pathS = path.string();

    if (!eng::ReadFile(buffer, path)) {
        return false;
    }

    VK_ASSERT_MSG(buffer.size() % sizeof(uint32_t) == 0, "Size of SPIR-V byte code of %s must be multiple of %zu", pathS.c_str(), sizeof(uint32_t));

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
        vkn::DescriptorInfo::Create(COMMON_MESH_INFOS_DESCRIPTOR_SLOT,   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_TRANSFORMS_DESCRIPTOR_SLOT,   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_MATERIALS_DESCRIPTOR_SLOT,    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_MTL_TEXTURES_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, COMMON_BINDLESS_TEXTURES_COUNT, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(COMMON_INST_INFOS_DESCRIPTOR_SLOT,   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_VERTEX_DATA_DESCRIPTOR_SLOT,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT),
        vkn::DescriptorInfo::Create(COMMON_DBG_TEXTURES_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, (uint32_t)COMMON_DBG_TEX_IDX::COUNT, VK_SHADER_STAGE_ALL),
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

    s_descSetLayouts[(size_t)PassID::ZPASS].Create(createInfo).SetDebugName("ZPASS_DESCRIPTOR_SET_LAYOUT");
}


static void CreateMeshCullingDescriptorSetLayout()
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
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[(size_t)PassID::MESH_CULLING].Create(createInfo).SetDebugName("MESH_CULLING_DESCRIPTOR_SET_LAYOUT");
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


static void CreateDescriptorBuffer()
{
    std::array<vkn::DescriptorSetLayout*, (size_t)PassID::COUNT> layouts = {};
    
    for (size_t i = 0; i < layouts.size(); ++i) {
        layouts[i] = &s_descSetLayouts[i];
    }

    s_descriptorBuffer.Create(&s_vkDevice, layouts).SetDebugName("COMMON_DESCRIPTOR_BUFFER");
}


static void CreateDescriptorSets()
{
    CreateCommonDescriptorSetLayout();
    CreateZPassDescriptorSetLayout();
    CreateMeshCullingDescriptorSetLayout();
    CreateGBufferDescriptorSetLayout();
    CreateDeferredLightingDescriptorSetLayout();
    CreatePostProcessingDescriptorSetLayout();
    CreateBackbufferPassDescriptorSetLayout();
    CreateSkyboxDescriptorSetLayout();
    CreateIrradianceMapGenDescriptorSetLayout();
    CreatePrefilteredEnvMapGenDescriptorSetLayout();
    CreateBRDFIntegrationLUTGenDescriptorSetLayout();

    CreateDescriptorBuffer();
}


static void CreateMeshCullingPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[] = {
        &s_descSetLayouts[(size_t)PassID::COMMON],
        &s_descSetLayouts[(size_t)PassID::MESH_CULLING]
    };

    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MESH_CULLING_PUSH_CONSTS) };

    s_PSOLayouts[(size_t)PassID::MESH_CULLING].Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1))
        .SetDebugName("MESH_CULLING_PIPELINE_LAYOUT");
}


static void CreateZPassPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[] = {
        &s_descSetLayouts[(size_t)PassID::COMMON],
        &s_descSetLayouts[(size_t)PassID::ZPASS]
    };

    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ZPASS_PUSH_CONSTS) };

    s_PSOLayouts[(size_t)PassID::ZPASS].Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1))
        .SetDebugName("ZPASS_PIPELINE_LAYOUT");
}


static void CreateGBufferPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[] = {
        &s_descSetLayouts[(size_t)PassID::COMMON],
        &s_descSetLayouts[(size_t)PassID::GBUFFER]
    };

    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GBUFFER_PUSH_CONSTS) };

    s_PSOLayouts[(size_t)PassID::GBUFFER].Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1)).SetDebugName("GBUFFER_PIPELINE_LAYOUT");
}


static void CreateDeferredLightingPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[] = {
        &s_descSetLayouts[(size_t)PassID::COMMON],
        &s_descSetLayouts[(size_t)PassID::DEFERRED_LIGHTING]
    };

    s_PSOLayouts[(size_t)PassID::DEFERRED_LIGHTING].Create(&s_vkDevice, layoutPtrs).SetDebugName("DEFERRED_LIGHTING_PIPELINE_LAYOUT");
}


static void CreatePostProcessingPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[] = {
        &s_descSetLayouts[(size_t)PassID::COMMON],
        &s_descSetLayouts[(size_t)PassID::POST_PROCESSING]
    };

    s_PSOLayouts[(size_t)PassID::POST_PROCESSING].Create(&s_vkDevice, layoutPtrs).SetDebugName("POST_PROCESSING_PIPELINE_LAYOUT");
}


static void CreateBackbufferPassPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[] = {
        &s_descSetLayouts[(size_t)PassID::COMMON],
        &s_descSetLayouts[(size_t)PassID::BACKBUFFER]
    };

    s_PSOLayouts[(size_t)PassID::BACKBUFFER].Create(&s_vkDevice, layoutPtrs).SetDebugName("BACKBUFFER_PIPELINE_LAYOUT");
}


static void CreateSkyboxPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[] = {
        &s_descSetLayouts[(size_t)PassID::COMMON],
        &s_descSetLayouts[(size_t)PassID::SKYBOX]
    };

    s_PSOLayouts[(size_t)PassID::SKYBOX].Create(&s_vkDevice, layoutPtrs).SetDebugName("SKYBOX_PIPELINE_LAYOUT");
}


static void CreateIrradianceMapGenPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[] = {
        &s_descSetLayouts[(size_t)PassID::COMMON],
        &s_descSetLayouts[(size_t)PassID::IRRADIANCE_MAP_GEN]
    };

    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(IRRADIANCE_MAP_PUSH_CONSTS) };

    s_PSOLayouts[(size_t)PassID::IRRADIANCE_MAP_GEN].Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1))
        .SetDebugName("IRRAD_MAP_GEN_PIPELINE_LAYOUT");
}


static void CreatePrefilteredEnvMapGenPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[] = {
        &s_descSetLayouts[(size_t)PassID::COMMON],
        &s_descSetLayouts[(size_t)PassID::PREFILT_ENV_MAP_GEN]
    };

    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PREFILTERED_ENV_MAP_PUSH_CONSTS) };

    s_PSOLayouts[(size_t)PassID::PREFILT_ENV_MAP_GEN].Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1))
        .SetDebugName("PREFILT_ENV_MAP_GET_PIPELINE_LAYOUT");
}


static void CreateBRDFIntegrationLUTGenPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[] = {
        &s_descSetLayouts[(size_t)PassID::COMMON],
        &s_descSetLayouts[(size_t)PassID::BRDF_LUT_GEN]
    };

    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BRDF_INTEGRATION_PUSH_CONSTS) };

    s_PSOLayouts[(size_t)PassID::BRDF_LUT_GEN].Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1))
        .SetDebugName("GRDF_LUT_GEN_PIPELINE_LAYOUT");
}


static void CreateMeshCullingPipeline(const fs::path& csPath)
{
    if (!LoadShaderSpirVCode(csPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", csPath.string().c_str());
    }
    
    vkn::Shader shader;
    shader.Create(&s_vkDevice, VK_SHADER_STAGE_COMPUTE_BIT, s_shaderCodeBuffer).SetDebugName("MESH_CULLING_COMPUTE_SHADER");

    vkn::PSO& pso = s_PSOs[(size_t)PassID::MESH_CULLING];

    pso = s_computePSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .SetShader(shader)
        .SetLayout(s_PSOLayouts[(size_t)PassID::MESH_CULLING])
        .Build();

    pso.SetDebugName("MESH_CULLING_PSO");
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

    vkn::PSO& pso = s_PSOs[(size_t)PassID::ZPASS];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[(size_t)PassID::ZPASS])
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(1.f)
    #ifdef ENG_REVERSED_Z
        .EnableDepthTest(VK_TRUE, VK_COMPARE_OP_GREATER_OR_EQUAL)
    #else
        .EnableDepthTest(VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
    #endif
        .EnableDepthBoundsTest(0.f, 1.f)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })        
        .SetDepthAttachmentFormat(s_commonDepthRT.GetFormat());
    
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
        .EnableDepthTest(VK_FALSE, VK_COMPARE_OP_EQUAL)
        .EnableDepthBoundsTest(0.f, 1.f)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR });

    #ifdef ENG_BUILD_DEBUG
        s_graphicsPSOBuilder.AddDynamicState(std::array{ VK_DYNAMIC_STATE_DEPTH_COMPARE_OP, VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE });
    #else
        s_graphicsPSOBuilder.EnableDepthTest(VK_FALSE, VK_COMPARE_OP_EQUAL);
    #endif

    for (const vkn::Texture& colorRT : s_gbufferRTs) {
        s_graphicsPSOBuilder.AddColorAttachment(colorRT.GetFormat()); 
    }
    s_graphicsPSOBuilder.SetDepthAttachmentFormat(s_commonDepthRT.GetFormat());
    
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
        .EnableDepthTest(VK_FALSE, VK_COMPARE_OP_GREATER_OR_EQUAL)
    #else
        .EnableDepthTest(VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL)
    #endif
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .AddColorAttachment(s_colorRT16F.GetFormat())
        .SetDepthAttachmentFormat(s_commonDepthRT.GetFormat());
    
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


static void CreatePipelines()
{
    CreateMeshCullingPipelineLayout();
    CreateZPassPipelineLayout();
    CreateGBufferPipelineLayout();
    CreateDeferredLightingPipelineLayout();
    CreatePostProcessingPipelineLayout();
    CreateBackbufferPassPipelineLayout();
    CreateSkyboxPipelineLayout();
    CreateIrradianceMapGenPipelineLayout();
    CreatePrefilteredEnvMapGenPipelineLayout();
    CreateBRDFIntegrationLUTGenPipelineLayout();
    CreateMeshCullingPipeline("shaders/bin/scene.cs.spv");
    CreateZPassPipeline("shaders/bin/zpass.vs.spv", "shaders/bin/zpass.ps.spv");
    CreateGBufferRenderPipeline("shaders/bin/gbuffer.vs.spv", "shaders/bin/gbuffer.ps.spv");
    CreateDeferredLightingPipeline("shaders/bin/deferred_lighting.vs.spv", "shaders/bin/deferred_lighting.ps.spv");
    CreatePostProcessingPipeline("shaders/bin/post_processing.vs.spv", "shaders/bin/post_processing.ps.spv");
    CreateBackbufferPassPipeline("shaders/bin/backbuffer.vs.spv", "shaders/bin/backbuffer.ps.spv");
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

    texCreateInfos[(size_t)COMMON_DBG_TEX_IDX::CHECKERBOARD].extent = { 64u, 64u, 1u };

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

        cmdBuffer.BeginBarrierList().AddTextureBarrier(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        
        cmdBuffer.CmdPushBarrierList();

        vkn::BufferToTextureCopyInfo copyInfo = {};
        copyInfo.texSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyInfo.texSubresource.mipLevel = 0;
        copyInfo.texSubresource.baseArrayLayer = 0;
        copyInfo.texSubresource.layerCount = 1;
        copyInfo.texExtent = texture.GetSize();

        cmdBuffer.CmdCopyBuffer(s_commonStagingBuffers[stagingBufIdx], texture, copyInfo);
    
        cmdBuffer.BeginBarrierList().AddTextureBarrier(texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        
        cmdBuffer.CmdPushBarrierList();
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
    createInfo.size = sizeof(glm::uint) + MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(COMMON_INDIRECT_DRAW_CMD);
    createInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT | 
        VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT;
    createInfo.pAllocInfo = &allocInfo;

    s_commonOpaqueMeshDrawCmdBuffer.Create(createInfo).SetDebugName("COMMON_OPAQUE_MESH_DRAW_CMD_BUFFER");

    createInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    createInfo.size = MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(glm::uint);
    
    s_commonCulledOpaqueInstInfoIDsBuffer.Create(createInfo).SetDebugName("COMMON_CULLED_OPAQUE_INST_INFO_IDS_BUFFER");


    createInfo.size = sizeof(glm::uint) + MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(COMMON_INDIRECT_DRAW_CMD);
    createInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT | 
        VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT;

    s_commonAKillMeshDrawCmdBuffer.Create(createInfo).SetDebugName("COMMON_AKILL_MESH_DRAW_CMD_BUFFER");

    createInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    createInfo.size = MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(glm::uint);
    
    s_commonCulledAKillInstInfoIDsBuffer.Create(createInfo).SetDebugName("COMMON_CULLED_AKILL_INST_INFO_IDS_BUFFER");


    createInfo.size = sizeof(glm::uint) + MAX_INDIRECT_DRAW_CMD_COUNT * sizeof(COMMON_INDIRECT_DRAW_CMD);
    createInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT | 
        VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT;

    s_commonTranspMeshDrawCmdBuffer.Create(createInfo).SetDebugName("COMMON_TRANSP_MESH_DRAW_CMD_BUFFER");

    createInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
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
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::ZPASS, 
        ZPASS_OPAQUE_INST_INFO_IDS_DESCRIPTOR_SLOT, 0, s_commonCulledOpaqueInstInfoIDsBuffer);
    
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::ZPASS, 
        ZPASS_AKILL_INST_INFO_IDS_DESCRIPTOR_SLOT, 0, s_commonCulledAKillInstInfoIDsBuffer);
}


static void WriteMeshCullingDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::MESH_CULLING, 
        MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, 0, s_commonOpaqueMeshDrawCmdBuffer);

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::MESH_CULLING, 
        MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, 0, s_commonAKillMeshDrawCmdBuffer);

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::MESH_CULLING, 
        MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_UAV_DESCRIPTOR_SLOT, 0, s_commonTranspMeshDrawCmdBuffer);

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::MESH_CULLING, 
        MESH_CULL_OPAQUE_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, 0, s_commonCulledOpaqueInstInfoIDsBuffer);

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::MESH_CULLING, 
        MESH_CULL_AKILL_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, 0, s_commonCulledAKillInstInfoIDsBuffer);

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::MESH_CULLING, 
        MESH_CULL_TRANSP_INST_INFO_IDS_UAV_DESCRIPTOR_SLOT, 0, s_commonCulledTranspInstInfoIDsBuffer);
}


static void WriteGBufferDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GBUFFER, 
        GBUFFER_OPAQUE_INST_INFO_IDS_DESCRIPTOR_SLOT, 0, s_commonCulledOpaqueInstInfoIDsBuffer);

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::GBUFFER, 
        GBUFFER_AKILL_INST_INFO_IDS_DESCRIPTOR_SLOT, 0, s_commonCulledAKillInstInfoIDsBuffer);
}


static void WriteDeferredLightingDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::DEFERRED_LIGHTING, 
        DEFERRED_LIGHTING_OUTPUT_UAV_DESCRIPTOR_SLOT, 0, s_colorRTView16F);

    std::array<vkn::TextureView*, GBUFFER_RT_COUNT> gbufferViews = {};
    for (size_t i = 0; i < GBUFFER_RT_COUNT; ++i) {
        gbufferViews[i] = &s_gbufferRTViews[i];
    }

    for (size_t i = 0; i < GBUFFER_RT_COUNT; ++i) {
        s_descriptorBuffer.WriteDescriptor((size_t)PassID::DEFERRED_LIGHTING, 
            DEFERRED_LIGHTING_GBUFFER_0_DESCRIPTOR_SLOT + i, 0, *gbufferViews[i]);
    }

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::DEFERRED_LIGHTING, 
        DEFERRED_LIGHTING_DEPTH_DESCRIPTOR_SLOT, 0, s_commonDepthRTView);

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::DEFERRED_LIGHTING, 
        DEFERRED_LIGHTING_IRRADIANCE_MAP_DESCRIPTOR_SLOT, 0, s_irradianceMapTextureView);
    
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::DEFERRED_LIGHTING, 
        DEFERRED_LIGHTING_PREFILTERED_ENV_MAP_DESCRIPTOR_SLOT, 0, s_prefilteredEnvMapTextureView);

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::DEFERRED_LIGHTING, 
        DEFERRED_LIGHTING_BRDF_LUT_DESCRIPTOR_SLOT, 0, s_brdfLUTTextureView);
}


static void WritePostProcessingDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::POST_PROCESSING,
        POST_PROCESSING_INPUT_COLOR_DESCRIPTOR_SLOT, 0, s_colorRTView16F);
}


static void WriteBackbufferPassDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::BACKBUFFER,
        BACKBUFFER_INPUT_COLOR_DESCRIPTOR_SLOT, 0, s_colorRTView8U);
}


static void WriteSkyboxDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::SKYBOX,
        SKYBOX_TEXTURE_DESCRIPTOR_SLOT, 0, s_skyboxTextureView);
}


static void WriteIrradianceMapGenDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::IRRADIANCE_MAP_GEN,
        IRRADIANCE_MAP_GEN_ENV_MAP_DESCRIPTOR_SLOT, 0, s_skyboxTextureView);

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::IRRADIANCE_MAP_GEN,
        IRRADIANCE_MAP_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT, 0, s_irradianceMapTextureViewRW);
}


static void WritePrefilteredEnvMapGenDescriptorSets()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::PREFILT_ENV_MAP_GEN,
        PREFILTERED_ENV_MAP_GEN_ENV_MAP_DESCRIPTOR_SLOT, 0, s_skyboxTextureView);
    
    for (uint32_t i = 0; i < s_prefilteredEnvMapTextureViewRWs.size(); ++i) {
        s_descriptorBuffer.WriteDescriptor((size_t)PassID::PREFILT_ENV_MAP_GEN,
            PREFILTERED_ENV_MAP_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT, i, s_prefilteredEnvMapTextureViewRWs[i]);
    }
}


static void WriteBRDFIntegrationLUTGenDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor((size_t)PassID::BRDF_LUT_GEN,
        BRDF_INTEGRATION_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT, 0, s_brdfLUTTextureViewRW);
}


static void WriteCommonDescriptorSet()
{
    for (size_t i = 0; i < s_commonSamplers.size(); ++i) {
        s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON,
            COMMON_SAMPLERS_DESCRIPTOR_SLOT, i, s_commonSamplers[i]);
    }

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON,
        COMMON_CONST_BUFFER_DESCRIPTOR_SLOT, 0, s_commonConstBuffer);

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON,
        COMMON_MESH_INFOS_DESCRIPTOR_SLOT, 0, s_commonMeshDataBuffer);

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON,
        COMMON_TRANSFORMS_DESCRIPTOR_SLOT, 0, s_commonTransformDataBuffer);

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON,
        COMMON_MATERIALS_DESCRIPTOR_SLOT, 0, s_commonMaterialDataBuffer);

    for (size_t i = 0; i < s_commonMaterialTextureViews.size(); ++i) {
        s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON,
            COMMON_MTL_TEXTURES_DESCRIPTOR_SLOT, i, s_commonMaterialTextureViews[i]);
    }

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON,
        COMMON_INST_INFOS_DESCRIPTOR_SLOT, 0, s_commonInstDataBuffer);

    s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON,
        COMMON_VERTEX_DATA_DESCRIPTOR_SLOT, 0, s_vertexBuffer);

#ifdef ENG_BUILD_DEBUG
    for (size_t i = 0; i < s_commonDbgTextureViews.size(); ++i) {
        s_descriptorBuffer.WriteDescriptor((size_t)PassID::COMMON,
            COMMON_DBG_TEXTURES_DESCRIPTOR_SLOT, i, s_commonDbgTextureViews[i]);
    }
#endif
}


static void WriteDescriptorSets()
{
    WriteCommonDescriptorSet();
    WriteZPassDescriptorSet();
    WriteMeshCullingDescriptorSet();
    WriteGBufferDescriptorSet();
    WriteDeferredLightingDescriptorSet();
    WritePostProcessingDescriptorSet();
    WriteBackbufferPassDescriptorSet();
    WriteSkyboxDescriptorSet();
    WriteIrradianceMapGenDescriptorSet();
    WritePrefilteredEnvMapGenDescriptorSets();
    WriteBRDFIntegrationLUTGenDescriptorSet();
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

    for (size_t sceneId = 0; sceneId < asset.scenes.size(); ++sceneId) {
        gltf::iterateSceneNodes(asset, sceneId, gltf::math::fmat4x4(1.f), [&](auto&& node, auto&& trs)
        {
            static_assert(sizeof(trs) == sizeof(glm::float4x4));
    
            glm::float4x4 transform(1.f);
            memcpy(&transform, &trs, sizeof(transform));
    
            s_cpuTransformData.emplace_back(transform);
    
            if (node.meshIndex.has_value()) {
                const uint32_t trsIdx = s_cpuTransformData.size() - 1;
                
                const uint32_t meshIdx = node.meshIndex.value();
                const gltf::Mesh& mesh = asset.meshes[meshIdx];

                const uint32_t baseIdx = meshBaseIdxOffsets[meshIdx];

                for (uint32_t i = 0; i < mesh.primitives.size(); ++i) {
                    const gltf::Primitive& primitive = mesh.primitives[i];

                    COMMON_INST_INFO instInfo = {};
                    
                    instInfo.TRANSFORM_IDX = trsIdx;
                    instInfo.MESH_IDX = baseIdx + i;
    
                    CORE_ASSERT_MSG(primitive.materialIndex.has_value(), "Some of mesh %s primitive doesn't have material", mesh.name.c_str());
                    instInfo.MATERIAL_IDX = primitive.materialIndex.value();

                    s_cpuInstData.emplace_back(instInfo);
                }
            }
        });
    }

    CORE_LOG_INFO("FastGLTF: Instance data loading finished: %f ms", timer.End().GetDuration<float, std::milli>());

    timer.Reset().Start();

    std::sort(s_cpuInstData.begin(), s_cpuInstData.end(), 
        [](const COMMON_INST_INFO& a, const COMMON_INST_INFO& b) {
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

    const size_t meshDataBufferSize = s_cpuMeshData.size() * sizeof(COMMON_MESH_INFO);
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

                cmdBuffer.BeginBarrierList().AddTextureBarrier(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
                cmdBuffer.CmdPushBarrierList();

                vkn::BufferToTextureCopyInfo copyInfo = {};
                copyInfo.texSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyInfo.texSubresource.mipLevel = 0;
                copyInfo.texSubresource.baseArrayLayer = 0;
                copyInfo.texSubresource.layerCount = 1;
                copyInfo.texExtent = texture.GetSize();

                cmdBuffer.CmdCopyBuffer(s_commonStagingBuffers[j], texture, copyInfo);

                const TextureLoadData& texData = s_cpuTexturesData[textureIdx];

                GenerateTextureMipmaps(cmdBuffer, texture, texData);

                cmdBuffer.BeginBarrierList().AddTextureBarrier(texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
                cmdBuffer.CmdPushBarrierList();
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

    vkn::Buffer& stagingBuffer = s_commonStagingBuffers[0];

    const size_t bufferSize = s_cpuInstData.size() * sizeof(COMMON_INST_INFO);
    CORE_ASSERT(bufferSize <= stagingBuffer.GetMemorySize());

    void* pData = stagingBuffer.Map();
    memcpy(pData, s_cpuInstData.data(), bufferSize);
    stagingBuffer.Unmap();

    vkn::AllocationInfo instInfosBufAllocInfo = {};
    instInfosBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    instInfosBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::BufferCreateInfo instInfosBufCreateInfo = {};
    instInfosBufCreateInfo.pDevice = &s_vkDevice;
    instInfosBufCreateInfo.size = bufferSize;
    instInfosBufCreateInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    instInfosBufCreateInfo.pAllocInfo = &instInfosBufAllocInfo;

    s_commonInstDataBuffer.Create(instInfosBufCreateInfo).SetDebugName("COMMON_INSTANCE_DATA");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        cmdBuffer.CmdCopyBuffer(stagingBuffer, s_commonInstDataBuffer, bufferSize);
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
    vkn::AllocationInfo allocInfo = {};
    allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    
    vkn::BufferCreateInfo createInfo = {};
    createInfo.pDevice = &s_vkDevice;
    createInfo.size = sizeof(COMMON_CB_DATA);
    createInfo.usage = VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    createInfo.pAllocInfo = &allocInfo;

    s_commonConstBuffer.Create(createInfo).SetDebugName("COMMON_CB");
}


void UpdateGPUCommonConstBuffer()
{
    ENG_PROFILE_SCOPED_MARKER_C("Update_Common_Const_Buffer", eng::ProfileColor::Cyan4);

    COMMON_CB_DATA* pCommonConstBufferData = (COMMON_CB_DATA*)s_commonConstBuffer.Map();

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


static void ResizeVkSwapchain(eng::Window& window)
{
    bool resizeSucceded;
    s_vkSwapchain.Resize(window.GetWidth(), window.GetHeight(), resizeSucceded);
    
    s_swapchainRecreateRequired = !resizeSucceded;
}


void UpdateScene()
{
    if (s_swapchainRecreateRequired) {
        s_vkDevice.WaitIdle();

        ResizeVkSwapchain(*s_pWnd);

        if (s_swapchainRecreateRequired) {
            s_skipRender = true;
        } else {
            ResizeDynamicRenderTargets();

            WriteDeferredLightingDescriptorSet();
            WritePostProcessingDescriptorSet();
            WriteBackbufferPassDescriptorSet();
        }    
    }

    const float moveDist = glm::length(s_cameraVel);

    if (!math::IsZero(moveDist)) {
        const glm::float3 moveDir = glm::normalize(s_camera.GetRotation() * (s_cameraVel / moveDist));
        s_camera.MoveAlongDir(moveDir, moveDist);
    }

    s_camera.Update();
}


static bool IsInstVisible(const COMMON_INST_INFO& instInfo)
{
    ENG_PROFILE_TRANSIENT_SCOPED_MARKER_C("CPU_Is_Inst_Visible", eng::ProfileColor::Purple1);

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

    cmdBuffer.BeginBarrierList().AddTextureBarrier(s_irradianceMapTexture, VK_IMAGE_LAYOUT_GENERAL, 
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    cmdBuffer.CmdPushBarrierList();

    cmdBuffer.CmdBindPSO(s_PSOs[(size_t)PassID::IRRADIANCE_MAP_GEN]);
    
    cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::IRRADIANCE_MAP_GEN], VK_PIPELINE_BIND_POINT_COMPUTE, 
        (size_t)PassID::COMMON, 0);
    cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::IRRADIANCE_MAP_GEN], VK_PIPELINE_BIND_POINT_COMPUTE, 
        (size_t)PassID::IRRADIANCE_MAP_GEN, 1);

    IRRADIANCE_MAP_PUSH_CONSTS pushConsts = {};
    pushConsts.ENV_MAP_FACE_SIZE.x = s_skyboxTexture.GetSizeX();
    pushConsts.ENV_MAP_FACE_SIZE.y = s_skyboxTexture.GetSizeY();

    cmdBuffer.CmdPushConstants(s_PSOLayouts[(size_t)PassID::IRRADIANCE_MAP_GEN], VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

    cmdBuffer.CmdDispatch(ceil(COMMON_IRRADIANCE_MAP_SIZE.x / 32.f), ceil(COMMON_IRRADIANCE_MAP_SIZE.y / 32.f), 6);

    cmdBuffer.BeginBarrierList().AddTextureBarrier(s_irradianceMapTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    cmdBuffer.CmdPushBarrierList();

    CORE_LOG_INFO("Irradiance map generation finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void PrecomputeIBLPrefilteredEnvMap(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Precompute_IBL_Prefiltered_Env_Map", eng::ProfileColor::OrangeRed);
    eng::Timer timer;

    cmdBuffer.CmdBindPSO(s_PSOs[(size_t)PassID::PREFILT_ENV_MAP_GEN]);

    PREFILTERED_ENV_MAP_PUSH_CONSTS pushConsts = {};
    pushConsts.ENV_MAP_FACE_SIZE.x = s_skyboxTexture.GetSizeX();
    pushConsts.ENV_MAP_FACE_SIZE.y = s_skyboxTexture.GetSizeY();

    cmdBuffer.BeginBarrierList().AddTextureBarrier(s_prefilteredEnvMapTexture, VK_IMAGE_LAYOUT_GENERAL, 
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    cmdBuffer.CmdPushBarrierList();

    cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::PREFILT_ENV_MAP_GEN], VK_PIPELINE_BIND_POINT_COMPUTE, 
        (size_t)PassID::COMMON, 0);

    cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::PREFILT_ENV_MAP_GEN], VK_PIPELINE_BIND_POINT_COMPUTE, 
        (size_t)PassID::PREFILT_ENV_MAP_GEN, 1);

    for (size_t mip = 0; mip < COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT; ++mip) {
        pushConsts.MIP = mip;

        cmdBuffer.CmdPushConstants(s_PSOLayouts[(size_t)PassID::PREFILT_ENV_MAP_GEN], VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

        const uint32_t sizeX = COMMON_PREFILTERED_ENV_MAP_SIZE.x >> mip;
        const uint32_t sizeY = COMMON_PREFILTERED_ENV_MAP_SIZE.y >> mip;

        cmdBuffer.CmdDispatch((uint32_t)ceil(sizeX / 32.f), (uint32_t)ceil(sizeY / 32.f), 6U);
    }

    cmdBuffer.BeginBarrierList().AddTextureBarrier(s_prefilteredEnvMapTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    cmdBuffer.CmdPushBarrierList();

    CORE_LOG_INFO("Prefiltered env map generation finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void PrecomputeIBLBRDFIntergrationLUT(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Precompute_IBL_BRDF_Intergration_LUT", eng::ProfileColor::OrangeRed);
    eng::Timer timer;

    cmdBuffer.CmdBindPSO(s_PSOs[(size_t)PassID::BRDF_LUT_GEN]);

    cmdBuffer.BeginBarrierList().AddTextureBarrier(s_brdfLUTTexture, VK_IMAGE_LAYOUT_GENERAL, 
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    cmdBuffer.CmdPushBarrierList();

    cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::BRDF_LUT_GEN], VK_PIPELINE_BIND_POINT_COMPUTE, 
        (size_t)PassID::COMMON, 0);
    cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::BRDF_LUT_GEN], VK_PIPELINE_BIND_POINT_COMPUTE, 
        (size_t)PassID::BRDF_LUT_GEN, 1);

    cmdBuffer.CmdDispatch((uint32_t)ceil(COMMON_BRDF_INTEGRATION_LUT_SIZE.x / 32.f), (uint32_t)ceil(COMMON_BRDF_INTEGRATION_LUT_SIZE.y / 32.f), 1U);

    cmdBuffer.BeginBarrierList().AddTextureBarrier(s_brdfLUTTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    cmdBuffer.CmdPushBarrierList();

    CORE_LOG_INFO("BRDF LUT generation finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


void MeshCullingPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_SCOPED_MARKER_C("Mesh_Culling_Pass", eng::ProfileColor::Blue3);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Mesh_Culling_Pass", eng::ProfileColor::Blue3);

    cmdBuffer.BeginBarrierList()
        .AddBufferBarrier(s_commonOpaqueMeshDrawCmdBuffer, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
        .AddBufferBarrier(s_commonAKillMeshDrawCmdBuffer, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
        .AddBufferBarrier(s_commonTranspMeshDrawCmdBuffer, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    cmdBuffer.CmdPushBarrierList();

    static constexpr VkDeviceSize COUNTER_OFFSET = 0; 
    static constexpr VkDeviceSize COUNTER_SIZE = sizeof(glm::uint);

    cmdBuffer.CmdFillBuffer(s_commonOpaqueMeshDrawCmdBuffer, 0, COUNTER_OFFSET, COUNTER_SIZE);
    cmdBuffer.CmdFillBuffer(s_commonAKillMeshDrawCmdBuffer,  0, COUNTER_OFFSET, COUNTER_SIZE);
    cmdBuffer.CmdFillBuffer(s_commonTranspMeshDrawCmdBuffer, 0, COUNTER_OFFSET, COUNTER_SIZE);

    cmdBuffer.BeginBarrierList()
        .AddBufferBarrier(s_commonOpaqueMeshDrawCmdBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
        .AddBufferBarrier(s_commonAKillMeshDrawCmdBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
        .AddBufferBarrier(s_commonTranspMeshDrawCmdBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
        .AddBufferBarrier(s_commonCulledOpaqueInstInfoIDsBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
        .AddBufferBarrier(s_commonCulledAKillInstInfoIDsBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
        .AddBufferBarrier(s_commonCulledTranspInstInfoIDsBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
    cmdBuffer.CmdPushBarrierList();

    cmdBuffer.CmdBindPSO(s_PSOs[(size_t)PassID::MESH_CULLING]);
    
    cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::MESH_CULLING], VK_PIPELINE_BIND_POINT_COMPUTE, 
        (size_t)PassID::COMMON, 0);
    cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::MESH_CULLING], VK_PIPELINE_BIND_POINT_COMPUTE, 
        (size_t)PassID::MESH_CULLING, 1);

    MESH_CULLING_PUSH_CONSTS pushConsts = {};
    pushConsts.INST_COUNT = s_cpuInstData.size();

    cmdBuffer.CmdPushConstants(s_PSOLayouts[(size_t)PassID::MESH_CULLING], VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

    cmdBuffer.CmdDispatch(ceil(s_cpuInstData.size() / 64.f), 1, 1);
}


void RenderPass_Depth(vkn::CmdBuffer& cmdBuffer, bool isAKillPass)
{
    if (!s_useDepthPass) {
        return;
    }

    vkn::BarrierList& barrierList = cmdBuffer.BeginBarrierList();

    barrierList.AddTextureBarrier(s_commonDepthRT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 
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
    
    cmdBuffer.CmdPushBarrierList();

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

        cmdBuffer.CmdBindPSO(s_PSOs[(size_t)PassID::ZPASS]);
        
        cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::ZPASS], VK_PIPELINE_BIND_POINT_GRAPHICS, 
            (size_t)PassID::COMMON, 0);
        cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::ZPASS], VK_PIPELINE_BIND_POINT_GRAPHICS, 
            (size_t)PassID::ZPASS, 1);

        cmdBuffer.CmdBindIndexBuffer(s_indexBuffer, 0, GetVkIndexType());

        ZPASS_PUSH_CONSTS pushConsts = {};
        pushConsts.IS_AKILL_PASS = isAKillPass;

        if (s_useMeshIndirectDraw) {
            cmdBuffer.CmdPushConstants(s_PSOLayouts[(size_t)PassID::ZPASS], VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);

            if (isAKillPass) {
                cmdBuffer.CmdDrawIndexedIndirect(s_commonAKillMeshDrawCmdBuffer, sizeof(glm::uint), s_commonAKillMeshDrawCmdBuffer, 0, 
                    MAX_INDIRECT_DRAW_CMD_COUNT, sizeof(COMMON_INDIRECT_DRAW_CMD));
            } else {
                cmdBuffer.CmdDrawIndexedIndirect(s_commonOpaqueMeshDrawCmdBuffer, sizeof(glm::uint), s_commonOpaqueMeshDrawCmdBuffer, 0, 
                    MAX_INDIRECT_DRAW_CMD_COUNT, sizeof(COMMON_INDIRECT_DRAW_CMD));
            }
        } else {
            ENG_PROFILE_SCOPED_MARKER_C("Depth_CPU_Frustum_Culling", eng::ProfileColor::Purple1);

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

                cmdBuffer.CmdPushConstants(s_PSOLayouts[(size_t)PassID::ZPASS], VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);

                const COMMON_MESH_INFO& mesh = s_cpuMeshData[s_cpuInstData[i].MESH_IDX];
                cmdBuffer.CmdDrawIndexed(mesh.INDEX_COUNT, 1, mesh.FIRST_INDEX, mesh.FIRST_VERTEX, i);
            }
        }
    cmdBuffer.CmdEndRendering();

    cmdBuffer.BeginBarrierList().AddTextureBarrier(s_commonDepthRT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
    cmdBuffer.CmdPushBarrierList();
}


void DepthPass(vkn::CmdBuffer& cmdBuffer)
{
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
        barrierList.AddTextureBarrier(s_commonDepthRT, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, 
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
    } else {
        barrierList.AddTextureBarrier(s_commonDepthRT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 
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

    cmdBuffer.CmdPushBarrierList();    

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

        cmdBuffer.CmdBindPSO(s_PSOs[(size_t)PassID::GBUFFER]);
        
        cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::GBUFFER], VK_PIPELINE_BIND_POINT_GRAPHICS, 
            (size_t)PassID::COMMON, 0);
        cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::GBUFFER], VK_PIPELINE_BIND_POINT_GRAPHICS, 
            (size_t)PassID::GBUFFER, 1);

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
            cmdBuffer.CmdPushConstants(s_PSOLayouts[(size_t)PassID::GBUFFER], VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);
            
            if (isAKillPass) {
                cmdBuffer.CmdDrawIndexedIndirect(s_commonAKillMeshDrawCmdBuffer, sizeof(glm::uint), s_commonAKillMeshDrawCmdBuffer, 0, 
                    MAX_INDIRECT_DRAW_CMD_COUNT, sizeof(COMMON_INDIRECT_DRAW_CMD));
            } else {
                cmdBuffer.CmdDrawIndexedIndirect(s_commonOpaqueMeshDrawCmdBuffer, sizeof(glm::uint), s_commonOpaqueMeshDrawCmdBuffer, 0, 
                    MAX_INDIRECT_DRAW_CMD_COUNT, sizeof(COMMON_INDIRECT_DRAW_CMD));
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

                cmdBuffer.CmdPushConstants(s_PSOLayouts[(size_t)PassID::GBUFFER], VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);
                
                const COMMON_MESH_INFO& mesh = s_cpuMeshData[s_cpuInstData[i].MESH_IDX];
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
    barrierList.AddTextureBarrier(s_commonDepthRT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

    cmdBuffer.CmdPushBarrierList();
    
    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = VkExtent2D { s_colorRT16F.GetSizeX(), s_colorRT16F.GetSizeY() };
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
        VkViewport viewport = {};
        viewport.width = renderingInfo.renderArea.extent.width;
        viewport.height = renderingInfo.renderArea.extent.height;
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        cmdBuffer.CmdSetViewport(0, 1, &viewport); 

        VkRect2D scissor = {};
        scissor.extent = renderingInfo.renderArea.extent;
        cmdBuffer.CmdSetScissor(0, 1, &scissor);

        cmdBuffer.CmdBindPSO(s_PSOs[(size_t)PassID::DEFERRED_LIGHTING]);
        
        cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::DEFERRED_LIGHTING], VK_PIPELINE_BIND_POINT_GRAPHICS, 
            (size_t)PassID::COMMON, 0);
        cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::DEFERRED_LIGHTING], VK_PIPELINE_BIND_POINT_GRAPHICS, 
            (size_t)PassID::DEFERRED_LIGHTING, 1);

        cmdBuffer.CmdDraw(6, 1, 0, 0);        
    cmdBuffer.CmdEndRendering();
}


void SkyboxPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_SCOPED_MARKER_C("Skybox_Pass", eng::ProfileColor::Aquamarine);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Skybox_Pass", eng::ProfileColor::Aquamarine);

    vkn::BarrierList& barrierList = cmdBuffer.BeginBarrierList();

    barrierList.AddTextureBarrier(s_colorRT16F, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    barrierList.AddTextureBarrier(s_commonDepthRT, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, 
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
    
    cmdBuffer.CmdPushBarrierList();

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };
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

        cmdBuffer.CmdBindPSO(s_PSOs[(size_t)PassID::SKYBOX]);
        
        cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::SKYBOX], VK_PIPELINE_BIND_POINT_GRAPHICS, 
            (size_t)PassID::COMMON, 0);
        cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::SKYBOX], VK_PIPELINE_BIND_POINT_GRAPHICS, 
            (size_t)PassID::SKYBOX, 1);

        cmdBuffer.CmdDraw(36, 1, 0, 0);        
    cmdBuffer.CmdEndRendering();
}


void PostProcessingPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_SCOPED_MARKER_C("Post_Processing_Pass", eng::ProfileColor::RebeccaPurple);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Post_Processing_Pass", eng::ProfileColor::RebeccaPurple);

    vkn::BarrierList& barrierList = cmdBuffer.BeginBarrierList();

    barrierList.AddTextureBarrier(s_colorRT16F, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    barrierList.AddTextureBarrier(s_colorRT8U, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    
    cmdBuffer.CmdPushBarrierList();

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };
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
        VkViewport viewport = {};
        viewport.width = renderingInfo.renderArea.extent.width;
        viewport.height = renderingInfo.renderArea.extent.height;
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        cmdBuffer.CmdSetViewport(0, 1, &viewport); 

        VkRect2D scissor = {};
        scissor.extent = renderingInfo.renderArea.extent;
        cmdBuffer.CmdSetScissor(0, 1, &scissor);

        cmdBuffer.CmdBindPSO(s_PSOs[(size_t)PassID::POST_PROCESSING]);
        
        cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::POST_PROCESSING], VK_PIPELINE_BIND_POINT_GRAPHICS, 
            (size_t)PassID::COMMON, 0);
        cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::POST_PROCESSING], VK_PIPELINE_BIND_POINT_GRAPHICS, 
            (size_t)PassID::POST_PROCESSING, 1);

        cmdBuffer.CmdDraw(6, 1, 0, 0);        
    cmdBuffer.CmdEndRendering();
}


#ifdef ENG_DEBUG_UI_ENABLED
static void DbgUIPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_SCOPED_MARKER_C("Dbg_UI_Render_Pass", eng::ProfileColor::Red);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Dbg_UI_Render_Pass", eng::ProfileColor::Red);

    s_dbgUI.BeginFrame(s_frameTime);

    DbgUI::FillData();

    cmdBuffer.BeginBarrierList().AddTextureBarrier(s_colorRT8U, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    cmdBuffer.CmdPushBarrierList();

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

    cmdBuffer.BeginBarrierList()
        .AddTextureBarrier(s_colorRT8U, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
        .AddTextureBarrier(scTexture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    cmdBuffer.CmdPushBarrierList();

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };
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
        VkViewport viewport = {};
        viewport.width = renderingInfo.renderArea.extent.width;
        viewport.height = renderingInfo.renderArea.extent.height;
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        cmdBuffer.CmdSetViewport(0, 1, &viewport); 

        VkRect2D scissor = {};
        scissor.extent = renderingInfo.renderArea.extent;
        cmdBuffer.CmdSetScissor(0, 1, &scissor);

        cmdBuffer.CmdBindPSO(s_PSOs[(size_t)PassID::BACKBUFFER]);
        
        cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::BACKBUFFER], VK_PIPELINE_BIND_POINT_GRAPHICS, 
            (size_t)PassID::COMMON, 0);
        cmdBuffer.CmdSetDescriptorBufferOffset(s_PSOLayouts[(size_t)PassID::BACKBUFFER], VK_PIPELINE_BIND_POINT_GRAPHICS, 
            (size_t)PassID::BACKBUFFER, 1);

        cmdBuffer.CmdDraw(6, 1, 0, 0);        
    cmdBuffer.CmdEndRendering();

    cmdBuffer.BeginBarrierList().AddTextureBarrier(scTexture, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_IMAGE_ASPECT_COLOR_BIT);
    cmdBuffer.CmdPushBarrierList();
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
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Render_Scene_GPU", eng::ProfileColor::DimGrey);

        cmdBuffer.CmdBindDescriptorBuffer(s_descriptorBuffer);

        MeshCullingPass(cmdBuffer);

        DepthPass(cmdBuffer);
        GBufferRenderPass(cmdBuffer);
        DeferredLightingPass(cmdBuffer);

        SkyboxPass(cmdBuffer);

        PostProcessingPass(cmdBuffer);

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

    // LoadScene(argc > 1 ? argv[1] : "../assets/Sponza/Sponza.gltf");
    // LoadScene(argc > 1 ? argv[1] : "../assets/LightSponza/Sponza.gltf");
    // LoadScene(argc > 1 ? argv[1] : "../assets/TestPBR/TestPBR.gltf");
    LoadScene(argc > 1 ? argv[1] : "../assets/GPUOcclusionTest/Occlusion.gltf");

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

    s_camera.SetPosition(glm::float3(0.f, 0.f, 4.f));
    s_camera.SetRotation(glm::quatLookAt(-M3D_AXIS_Z, M3D_AXIS_Y));
    s_camera.SetPerspProjection(glm::radians(90.f), (float)s_pWnd->GetWidth() / s_pWnd->GetHeight(), 0.01f, 10'000.f);

    s_pWnd->SetVisible(true);

    while(!s_pWnd->IsClosed()) {
        ProcessFrame();
    }

    s_vkDevice.WaitIdle();

    return 0;
}