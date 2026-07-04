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

#include <meshoptimizer.h>


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


struct PLANE
{
    float3 normal;
    float distance;
};


struct FRUSTUM
{
    PLANE planes[6];
};


static const uint COMMON_CSM_CASCADE_COUNT = 3;


enum class COMMON_MATERIAL_FLAGS : glm::uint
{
    DOUBLE_SIDED = 0x1,
    ALPHA_KILL = 0x2,
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

    uint2 PADDING;
    int EMISSIVE_TEX_IDX = -1;
    uint FLAGS;
};


struct COMMON_MESH_LOD
{
    uint FIRST_INDEX;
    uint INDEX_COUNT;
};


struct COMMON_MESH
{
    void PackAABB_LCS(const float3& min, const float3& max)
    {
        AABB_MIN_MAX_LCS_PACKED.x = glm::packHalf2x16(float2(min.x, min.y));
        AABB_MIN_MAX_LCS_PACKED.y = glm::packHalf2x16(float2(min.z, max.x));
        AABB_MIN_MAX_LCS_PACKED.z = glm::packHalf2x16(float2(max.y, max.z));
    }

    math::AABB GetAABB_LCS() const
    {
        return math::AABB(
            float3(glm::unpackHalf2x16(AABB_MIN_MAX_LCS_PACKED.x), glm::unpackHalf2x16(AABB_MIN_MAX_LCS_PACKED.y).x),
            float3(glm::unpackHalf2x16(AABB_MIN_MAX_LCS_PACKED.y).y, glm::unpackHalf2x16(AABB_MIN_MAX_LCS_PACKED.z))
        );
    }

    uint FIRST_VERTEX;
    uint VERTEX_COUNT;

    uint FIRST_LOD; // Index of mesh LOD 0 inside LOD buffer
    uint LOD_COUNT;

    uint3 AABB_MIN_MAX_LCS_PACKED; // x - MIN.xy, y - MIN.z and MAX.x, z - MAX.yz
    uint  PADDING;
};


struct COMMON_INST
{
    void PackAABB_WCS(const math::AABB& aabb)
    {
        AABB_MIN_MAX_WCS_PACKED.x = glm::packHalf2x16(float2(aabb.min.x, aabb.min.y));
        AABB_MIN_MAX_WCS_PACKED.y = glm::packHalf2x16(float2(aabb.min.z, aabb.max.x));
        AABB_MIN_MAX_WCS_PACKED.z = glm::packHalf2x16(float2(aabb.max.y, aabb.max.z));
    }

    math::AABB GetAABB_WCS() const
    {
        const float3 minn = float3(glm::unpackHalf2x16(AABB_MIN_MAX_WCS_PACKED.x), glm::unpackHalf2x16(AABB_MIN_MAX_WCS_PACKED.y).x);
        const float3 maxx = float3(glm::unpackHalf2x16(AABB_MIN_MAX_WCS_PACKED.y).y, glm::unpackHalf2x16(AABB_MIN_MAX_WCS_PACKED.z));

        return math::AABB(minn, maxx);
    }

    float3x4 MATR_WCS;

    uint3 AABB_MIN_MAX_WCS_PACKED; // x - MIN.xy, y - MIN.z and MAX.x, z - MAX.yz
    uint  PADDING_0;

    uint MESH_IDX;
    uint MATERIAL_IDX;
    uint2 PADDING_1;
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

static_assert(sizeof(COMMON_CMD_DRAW_INDEXED_INDIRECT) == sizeof(VkDrawIndexedIndirectCommand));


struct COMMON_CMD_DISPATCH_INDIRECT
{
    // NOTE: Don't change order of this variables!!!
    uint GROUP_SIZE_X;
    uint GROUP_SIZE_Y;
    uint GROUP_SIZE_Z;
};

static_assert(sizeof(COMMON_CMD_DISPATCH_INDIRECT) == sizeof(VkDispatchIndirectCommand));


enum COMMON_SAMPLER_IDX : uint32_t
{
    SAMPLER_IDX_NEAREST_REPEAT,
    SAMPLER_IDX_NEAREST_MIRRORED_REPEAT,
    SAMPLER_IDX_NEAREST_CLAMP_TO_EDGE,
    SAMPLER_IDX_NEAREST_CLAMP_TO_BORDER,
    SAMPLER_IDX_NEAREST_MIRROR_CLAMP_TO_EDGE,

    SAMPLER_IDX_LINEAR_REPEAT,
    SAMPLER_IDX_LINEAR_MIRRORED_REPEAT,
    SAMPLER_IDX_LINEAR_CLAMP_TO_EDGE,
    SAMPLER_IDX_LINEAR_CLAMP_TO_BORDER,
    SAMPLER_IDX_LINEAR_MIRROR_CLAMP_TO_EDGE,

    SAMPLER_IDX_ANISO_2X_NEAREST_REPEAT,
    SAMPLER_IDX_ANISO_2X_NEAREST_MIRRORED_REPEAT,
    SAMPLER_IDX_ANISO_2X_NEAREST_CLAMP_TO_EDGE,
    SAMPLER_IDX_ANISO_2X_NEAREST_CLAMP_TO_BORDER,
    SAMPLER_IDX_ANISO_2X_NEAREST_MIRROR_CLAMP_TO_EDGE,

    SAMPLER_IDX_ANISO_2X_LINEAR_REPEAT,
    SAMPLER_IDX_ANISO_2X_LINEAR_MIRRORED_REPEAT,
    SAMPLER_IDX_ANISO_2X_LINEAR_CLAMP_TO_EDGE,
    SAMPLER_IDX_ANISO_2X_LINEAR_CLAMP_TO_BORDER,
    SAMPLER_IDX_ANISO_2X_LINEAR_MIRROR_CLAMP_TO_EDGE,

    SAMPLER_IDX_ANISO_4X_NEAREST_REPEAT,
    SAMPLER_IDX_ANISO_4X_NEAREST_MIRRORED_REPEAT,
    SAMPLER_IDX_ANISO_4X_NEAREST_CLAMP_TO_EDGE,
    SAMPLER_IDX_ANISO_4X_NEAREST_CLAMP_TO_BORDER,
    SAMPLER_IDX_ANISO_4X_NEAREST_MIRROR_CLAMP_TO_EDGE,

    SAMPLER_IDX_ANISO_4X_LINEAR_REPEAT,
    SAMPLER_IDX_ANISO_4X_LINEAR_MIRRORED_REPEAT,
    SAMPLER_IDX_ANISO_4X_LINEAR_CLAMP_TO_EDGE,
    SAMPLER_IDX_ANISO_4X_LINEAR_CLAMP_TO_BORDER,
    SAMPLER_IDX_ANISO_4X_LINEAR_MIRROR_CLAMP_TO_EDGE,

    SAMPLER_IDX_COUNT
};


enum COMMON_GEOM_STREAM
{
    COMMON_GEOM_STREAM_POSITION,
    COMMON_GEOM_STREAM_NORMAL,
    COMMON_GEOM_STREAM_UV,
    COMMON_GEOM_STREAM_TANGENT,
    COMMON_GEOM_STREAM_COUNT
};


enum DBG_RT_VIEW_TYPE : uint32_t
{
    DBG_RT_VIEW_TYPE_NONE,
    DBG_RT_VIEW_TYPE_COMMON_DEPTH,
    DBG_RT_VIEW_TYPE_COMMON_HZB,
    DBG_RT_VIEW_TYPE_GBUFFER_ALBEDO,
    DBG_RT_VIEW_TYPE_GBUFFER_NORMAL,
    DBG_RT_VIEW_TYPE_GBUFFER_ROUGHNESS,
    DBG_RT_VIEW_TYPE_GBUFFER_METALNESS,
    DBG_RT_VIEW_TYPE_GBUFFER_AO,
    DBG_RT_VIEW_TYPE_GBUFFER_EMISSIVE,
    DBG_RT_VIEW_TYPE_IRRADIANCE_MAP,
    DBG_RT_VIEW_TYPE_PREFILTERED_ENV_MAP,
    DBG_RT_VIEW_TYPE_BRDF_LUT,
    DBG_RT_VIEW_TYPE_SKYBOX,
    DBG_RT_VIEW_TYPE_CSM_DEPTH,
    DBG_RT_VIEW_TYPE_CSM_CASCADE,

    DBG_RT_VIEW_TYPE_COUNT
};


enum COMMON_DBG_TEX_IDX : uint32_t
{
    COMMON_DBG_TEX_IDX_RED,
    COMMON_DBG_TEX_IDX_GREEN,
    COMMON_DBG_TEX_IDX_BLUE,
    COMMON_DBG_TEX_IDX_BLACK,
    COMMON_DBG_TEX_IDX_WHITE,
    COMMON_DBG_TEX_IDX_GREY,
    COMMON_DBG_TEX_IDX_CHECKERBOARD,
    COMMON_DBG_TEX_IDX_COUNT
};


struct COMMON_CB_DATA
{
    FRUSTUM  CSM_VIEW_FRUSTUMS[COMMON_CSM_CASCADE_COUNT];
    float4x4 CSM_VIEW_MATRICES[COMMON_CSM_CASCADE_COUNT];
    float4x4 CSM_VIEW_PROJ_MATRICES[COMMON_CSM_CASCADE_COUNT];
    float4   CSM_CASCADE_DISTANCES;

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
};

static_assert(sizeof(COMMON_CB_DATA::CSM_CASCADE_DISTANCES) >= sizeof(float[COMMON_CSM_CASCADE_COUNT]));


struct COMMON_DBG_CB_DATA
{
    int  FORCED_GEOM_LOD;
    uint FLAGS_0;

    DBG_RT_VIEW_TYPE RT_VIEW_TYPE;

    uint PADDING_0;
};


struct DBG_LINE_DATA
{
    uint COLOR;
};


struct DBG_TRIANGLE_DATA
{
    uint COLOR;
};


enum GEOM_QUEUE
{
    GEOM_QUEUE_OPAQUE,
    GEOM_QUEUE_AKILL,
    GEOM_QUEUE_COUNT
};


static const uint CSM_BUFFER_COUNT = COMMON_CSM_CASCADE_COUNT * GEOM_QUEUE_COUNT;


struct GEOM_BATCH
{
    uint MESH_ID;
    uint LOD_ID;
    uint FIRST_INST;
    uint INST_COUNT;
};


struct GEOM_CULLING_PER_DRAW_DATA
{
    uint HZB_MIPS_COUNT;
};


struct GEOM_BATCH_PER_DRAW_DATA
{
    uint PADDING;
};


struct GEOM_DRAW_CMD_GEN_PER_DRAW_DATA
{
    uint PADDING;
};


struct ZPASS_PER_DRAW_DATA
{
    uint IS_AKILL_PASS;
};


struct HZB_GEN_PER_DRAW_DATA
{
    uint2 SRC_MIP_RESOLUTION;
    uint2 DST_MIP_RESOLUTION;
    uint  DST_MIP_IDX;
};


struct CSM_PER_DRAW_DATA
{
    uint CASCADE_IDX;
    GEOM_QUEUE QUEUE;
};


struct GBUFFER_PER_DRAW_DATA
{
    uint IS_AKILL_PASS;
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


struct DBG_RT_VIEW_PER_DRAW_DATA
{
    uint MIP;
    uint FACE;
    uint CSM_CASCADE_IDX;
    float DEPTH_Z_NEAR;
    float DEPTH_Z_FAR;
};


static constexpr const char* DBG_RT_OUTPUT_NAMES[] = {
    "NONE",
    "COMMON_DEPTH",
    "COMMON_HZB",
    "GBUFFER_ALBEDO",
    "GBUFFER_NORMAL",
    "GBUFFER_ROUGHNESS",
    "GBUFFER_METALNESS",
    "GBUFFER_AO",
    "GBUFFER_EMISSIVE",
    "IRRADIANCE_MAP",
    "PREFILTERED_ENV_MAP",
    "BRDF_LUT",
    "SKYBOX",
    "CSM_DEPTH",
    "CSM_CASCADE",
};

static_assert(DBG_RT_VIEW_TYPE_COUNT == _countof(DBG_RT_OUTPUT_NAMES));


enum class TonemapPreset
{
    ACES,
    REINHARD,
    PARTIAL_UNCHARTED_2,
    UNCHARTED_2,
    COUNT
};


static constexpr const char* DBG_TONEMAPPING_NAMES[(size_t)TonemapPreset::COUNT] = {
    "ACES",
    "REINHARD",
    "PARTIAL UNCHARTED 2",
    "UNCHARTED 2",
};


static constexpr const char* COMMON_GEOM_STREAM_DBG_NAMES[] = {
    "POSITION",
    "NORMAL",
    "UV",
    "TANGENT",
};

static_assert(COMMON_GEOM_STREAM_COUNT == _countof(COMMON_GEOM_STREAM_DBG_NAMES));


static constexpr const char* GEOM_QUEUE_DBG_NAMES[] = {
    "OPAQUE",
    "AKILL",
};


static_assert(GEOM_QUEUE_COUNT == _countof(GEOM_QUEUE_DBG_NAMES));


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
};

static_assert(SAMPLER_IDX_COUNT == _countof(COMMON_SAMPLERS_DBG_NAMES));


enum PassID : uint32_t
{
    PASS_ID_COMMON,

    PASS_ID_GEOM_CULLING_PHASE_1,
    PASS_ID_GEOM_CULLING_PHASE_2,
    PASS_ID_GEOM_BATCHING,
    PASS_ID_GEOM_DRAW_CMD_GEN,
    
    PASS_ID_DEPTH,
    
    PASS_ID_HZB_GEN,

    PASS_ID_CSM_GEOM_CULLING,
    PASS_ID_CSM_GEOM_BATCHING,
    PASS_ID_CSM_GEOM_DRAW_CMD_GEN,
    PASS_ID_CSM_RENDER,
    
    PASS_ID_GBUFFER,
    
    PASS_ID_DEFERRED_LIGHTING,
    
    PASS_ID_SKYBOX,
    
    PASS_ID_POST_PROCESSING,
    
    PASS_ID_BACKBUFFER,
    
    PASS_ID_IRRADIANCE_MAP_GEN,
    PASS_ID_BRDF_LUT_GEN,
    PASS_ID_PREFILT_ENV_MAP_GEN,

#ifdef ENG_DEBUG_DRAW_ENABLED
    PASS_ID_DBG_DRAW_LINES,
    PASS_ID_DBG_DRAW_TRIANGLES,
    PASS_ID_DBG_RT_VIEW,
#endif

    PASS_ID_COUNT,
};


static constexpr const char* PASS_DBG_NAME[] = {
    "PASS_ID_COMMON",

    "PASS_ID_GEOM_CULLING_PHASE_1",
    "PASS_ID_GEOM_CULLING_PHASE_2",
    "PASS_ID_GEOM_BATCHING",
    "PASS_ID_GEOM_DRAW_CMD_GEN",
    
    "PASS_ID_DEPTH",
    
    "PASS_ID_HZB_GEN",

    "PASS_ID_CSM_GEOM_CULLING",
    "PASS_ID_CSM_GEOM_BATCHING",
    "PASS_ID_CSM_GEOM_DRAW_CMD_GEN",
    "PASS_ID_CSM_RENDER",
    
    "PASS_ID_GBUFFER",
    
    "PASS_ID_DEFERRED_LIGHTING",
    
    "PASS_ID_SKYBOX",
    
    "PASS_ID_POST_PROCESSING",
    
    "PASS_ID_BACKBUFFER",
    
    "PASS_ID_IRRADIANCE_MAP_GEN",
    "PASS_ID_BRDF_LUT_GEN",
    "PASS_ID_PREFILT_ENV_MAP_GEN",

#ifdef ENG_DEBUG_DRAW_ENABLED
    "PASS_ID_DBG_DRAW_LINES",
    "PASS_ID_DBG_DRAW_TRIANGLES",
    "PASS_ID_DBG_RT_VIEW",
#endif
};

static_assert(PASS_ID_COUNT == _countof(PASS_DBG_NAME));


enum DescSetID : uint32_t
{
    DESC_SET_ID_COMMON,

    DESC_SET_ID_GEOM_CULLING_PHASE_1,
    DESC_SET_ID_GEOM_CULLING_PHASE_2,
    DESC_SET_ID_GEOM_BATCHING_OPAQUE_PHASE_1,
    DESC_SET_ID_GEOM_BATCHING_OPAQUE_PHASE_2,
    DESC_SET_ID_GEOM_BATCHING_AKILL_PHASE_1,
    DESC_SET_ID_GEOM_BATCHING_AKILL_PHASE_2,   
    DESC_SET_ID_GEOM_DRAW_CMD_GEN_OPAQUE_PHASE_1,
    DESC_SET_ID_GEOM_DRAW_CMD_GEN_OPAQUE_PHASE_2,
    DESC_SET_ID_GEOM_DRAW_CMD_GEN_AKILL_PHASE_1,
    DESC_SET_ID_GEOM_DRAW_CMD_GEN_AKILL_PHASE_2,
    
    DESC_SET_ID_DEPTH_OPAQUE_PHASE_1,
    DESC_SET_ID_DEPTH_OPAQUE_PHASE_2,
    DESC_SET_ID_DEPTH_AKILL_PHASE_1,
    DESC_SET_ID_DEPTH_AKILL_PHASE_2,
    
    DESC_SET_ID_HZB_GEN,

    DESC_SET_ID_CSM_GEOM_CULLING,
    DESC_SET_ID_CSM_GEOM_BATCHING_OPAQUE_CASCADE_0,
    DESC_SET_ID_CSM_GEOM_BATCHING_OPAQUE_CASCADE_1,
    DESC_SET_ID_CSM_GEOM_BATCHING_OPAQUE_CASCADE_2,
    DESC_SET_ID_CSM_GEOM_BATCHING_AKILL_CASCADE_0,
    DESC_SET_ID_CSM_GEOM_BATCHING_AKILL_CASCADE_1,
    DESC_SET_ID_CSM_GEOM_BATCHING_AKILL_CASCADE_2,
    DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_OPAQUE_CASCADE_0,
    DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_OPAQUE_CASCADE_1,
    DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_OPAQUE_CASCADE_2,
    DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_AKILL_CASCADE_0,
    DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_AKILL_CASCADE_1,
    DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_AKILL_CASCADE_2,
    DESC_SET_ID_CSM_RENDER,

    DESC_SET_ID_GBUFFER_OPAQUE_PHASE_1,
    DESC_SET_ID_GBUFFER_OPAQUE_PHASE_2,
    DESC_SET_ID_GBUFFER_AKILL_PHASE_1,
    DESC_SET_ID_GBUFFER_AKILL_PHASE_2,
    
    DESC_SET_ID_DEFERRED_LIGHTING,
    
    DESC_SET_ID_SKYBOX,
    
    DESC_SET_ID_POST_PROCESSING,
    
    DESC_SET_ID_BACKBUFFER,
    
    DESC_SET_ID_IRRADIANCE_MAP_GEN,
    DESC_SET_ID_BRDF_LUT_GEN,
    DESC_SET_ID_PREFILT_ENV_MAP_GEN,

#ifdef ENG_DEBUG_DRAW_ENABLED
    DESC_SET_ID_DBG_DRAW_LINES,
    DESC_SET_ID_DBG_DRAW_TRIANGLES,
    DESC_SET_ID_DBG_RT_VIEW,
#endif

    DESC_SET_ID_COUNT,
};


static constexpr const char* DESC_SET_DBG_NAME[] = {
    "DESC_SET_ID_COMMON",

    "DESC_SET_ID_GEOM_CULLING_PHASE_1",
    "DESC_SET_ID_GEOM_CULLING_PHASE_2",
    "DESC_SET_ID_GEOM_BATCHING_OPAQUE_PHASE_1",
    "DESC_SET_ID_GEOM_BATCHING_OPAQUE_PHASE_2",
    "DESC_SET_ID_GEOM_BATCHING_AKILL_PHASE_1",
    "DESC_SET_ID_GEOM_BATCHING_AKILL_PHASE_2",   
    "DESC_SET_ID_GEOM_DRAW_CMD_GEN_OPAQUE_PHASE_1",
    "DESC_SET_ID_GEOM_DRAW_CMD_GEN_OPAQUE_PHASE_2",
    "DESC_SET_ID_GEOM_DRAW_CMD_GEN_AKILL_PHASE_1",
    "DESC_SET_ID_GEOM_DRAW_CMD_GEN_AKILL_PHASE_2",
    
    "DESC_SET_ID_DEPTH_OPAQUE_PHASE_1",
    "DESC_SET_ID_DEPTH_OPAQUE_PHASE_2",
    "DESC_SET_ID_DEPTH_AKILL_PHASE_1",
    "DESC_SET_ID_DEPTH_AKILL_PHASE_2",
    
    "DESC_SET_ID_HZB_GEN",

    "DESC_SET_ID_CSM_GEOM_CULLING",
    "DESC_SET_ID_CSM_GEOM_BATCHING_OPAQUE_CASCADE_0",
    "DESC_SET_ID_CSM_GEOM_BATCHING_OPAQUE_CASCADE_1",
    "DESC_SET_ID_CSM_GEOM_BATCHING_OPAQUE_CASCADE_2",
    "DESC_SET_ID_CSM_GEOM_BATCHING_AKILL_CASCADE_0",
    "DESC_SET_ID_CSM_GEOM_BATCHING_AKILL_CASCADE_1",
    "DESC_SET_ID_CSM_GEOM_BATCHING_AKILL_CASCADE_2",
    "DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_OPAQUE_CASCADE_0",
    "DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_OPAQUE_CASCADE_1",
    "DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_OPAQUE_CASCADE_2",
    "DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_AKILL_CASCADE_0",
    "DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_AKILL_CASCADE_1",
    "DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_AKILL_CASCADE_2",
    "DESC_SET_ID_CSM_RENDER",

    "DESC_SET_ID_GBUFFER_OPAQUE_PHASE_1",
    "DESC_SET_ID_GBUFFER_OPAQUE_PHASE_2",
    "DESC_SET_ID_GBUFFER_AKILL_PHASE_1",
    "DESC_SET_ID_GBUFFER_AKILL_PHASE_2",
    
    "DESC_SET_ID_DEFERRED_LIGHTING",
    
    "DESC_SET_ID_SKYBOX",
    
    "DESC_SET_ID_POST_PROCESSING",
    
    "DESC_SET_ID_BACKBUFFER",
    
    "DESC_SET_ID_IRRADIANCE_MAP_GEN",
    "DESC_SET_ID_BRDF_LUT_GEN",
    "DESC_SET_ID_PREFILT_ENV_MAP_GEN",

#ifdef ENG_DEBUG_DRAW_ENABLED
    "DESC_SET_ID_DBG_DRAW_LINES",
    "DESC_SET_ID_DBG_DRAW_TRIANGLES",
    "DESC_SET_ID_DBG_RT_VIEW",
#endif
};

static_assert(DESC_SET_ID_COUNT == _countof(DESC_SET_DBG_NAME));


static constexpr size_t COMMON_SAMPLERS_DESCRIPTOR_SLOT = 0;
static constexpr size_t COMMON_DBG_TEXTURES_DESCRIPTOR_SLOT = 1;
static constexpr size_t COMMON_CB_DESCRIPTOR_SLOT = 2;
static constexpr size_t COMMON_DBG_CB_DESCRIPTOR_SLOT = 3;
static constexpr size_t COMMON_GEOM_STREAMS_DESCRIPTOR_SLOT = 4;
static constexpr size_t COMMON_MESH_LOD_BUFFER_DESCRIPTOR_SLOT = 5;
static constexpr size_t COMMON_MESH_BUFFER_DESCRIPTOR_SLOT = 6;
static constexpr size_t COMMON_MATERIALS_DESCRIPTOR_SLOT = 7;
static constexpr size_t COMMON_MTL_TEXTURES_DESCRIPTOR_SLOT = 8;
static constexpr size_t COMMON_INST_BUFFER_DESCRIPTOR_SLOT = 9;
static constexpr size_t COMMON_DEPTH_DESCRIPTOR_SLOT = 10;
static constexpr size_t COMMON_HZB_DESCRIPTOR_SLOT = 11;

static constexpr size_t GEOM_CULL_VIS_INST_ID_QUEUES_UAV_DESCRIPTOR_SLOT = 0;
static constexpr size_t GEOM_CULL_VIS_INST_ID_QUEUE_SIZES_UAV_DESCRIPTOR_SLOT = 1;
static constexpr size_t GEOM_CULL_VIS_FLAGS_UAV_DESCRIPTOR_SLOT = 2;
static constexpr size_t GEOM_CULL_BATCH_DISPATCH_CMDS_UAV_DESCRIPTOR_SLOT = 3;

static constexpr size_t GEOM_BATCH_VIS_INST_ID_QUEUE_DESCRIPTOR_SLOT = 0;
static constexpr size_t GEOM_BATCH_VIS_INST_ID_QUEUE_SIZE_DESCRIPTOR_SLOT = 1;
static constexpr size_t GEOM_BATCH_BATCH_QUEUE_UAV_DESCRIPTOR_SLOT = 2;
static constexpr size_t GEOM_BATCH_BATCH_QUEUE_SIZE_UAV_DESCRIPTOR_SLOT = 3;
static constexpr size_t GEOM_BATCH_SORTED_VIS_INST_ID_QUEUE_UAV_DESCRIPTOR_SLOT = 4;
static constexpr size_t GEOM_BATCH_SORTED_VIS_INST_ID_QUEUE_SIZE_UAV_DESCRIPTOR_SLOT = 5;
static constexpr size_t GEOM_BATCH_DRAW_CMD_GEN_DISPATCH_CMD_UAV_DESCRIPTOR_SLOT = 6;

static constexpr size_t GEOM_DRAW_CMD_GEN_BATCH_QUEUE_DESCRIPTOR_SLOT = 0;
static constexpr size_t GEOM_DRAW_CMD_GEN_BATCH_QUEUE_SIZE_DESCRIPTOR_SLOT = 1;
static constexpr size_t GEOM_DRAW_CMD_GEN_CMD_QUEUE_UAV_DESCRIPTOR_SLOT = 2;

static constexpr size_t ZPASS_INST_ID_QUEUE_DESCRIPTOR_SLOT = 0;

static constexpr size_t HZB_SRC_MIPS_DESCRIPTOR_SLOT = 0;
static constexpr size_t HZB_DST_MIPS_UAV_DESCRIPTOR_SLOT = 1;

static constexpr size_t CSM_VIS_INST_ID_QUEUES_UAV_DESCRIPTOR_SLOT = 0;
static constexpr size_t CSM_VIS_INST_ID_QUEUE_SIZES_UAV_DESCRIPTOR_SLOT = 1;
static constexpr size_t CSM_BATCH_DISPATCH_CMDS_UAV_DESCRIPTOR_SLOT = 2;
static constexpr size_t CSM_INST_ID_QUEUE_DESCRIPTOR_SLOT = 3;

static constexpr size_t GBUFFER_INST_ID_QUEUE_DESCRIPTOR_SLOT = 0;

static constexpr size_t DEFERRED_LIGHTING_GBUFFER_0_DESCRIPTOR_SLOT = 0;
static constexpr size_t DEFERRED_LIGHTING_GBUFFER_1_DESCRIPTOR_SLOT = 1;
static constexpr size_t DEFERRED_LIGHTING_GBUFFER_2_DESCRIPTOR_SLOT = 2;
static constexpr size_t DEFERRED_LIGHTING_GBUFFER_3_DESCRIPTOR_SLOT = 3;
static constexpr size_t DEFERRED_LIGHTING_DEPTH_DESCRIPTOR_SLOT = 4;
static constexpr size_t DEFERRED_LIGHTING_IRRADIANCE_MAP_DESCRIPTOR_SLOT = 5;
static constexpr size_t DEFERRED_LIGHTING_PREFILTERED_ENV_MAP_DESCRIPTOR_SLOT = 6;
static constexpr size_t DEFERRED_LIGHTING_BRDF_LUT_DESCRIPTOR_SLOT = 7;
static constexpr size_t DEFERRED_LIGHTING_CSM_DESCRIPTOR_SLOT = 8;

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

static constexpr size_t DBG_DRAW_TRIANGLES_VERTEX_BUFFER_DESCRIPTOR_SLOT = 0;
static constexpr size_t DBG_DRAW_TRIANGLES_DATA_DESCRIPTOR_SLOT = 1;

static constexpr size_t DBG_RT_VIEW_COMMON_DEPTH_DESCRIPTOR_SLOT = 0;
static constexpr size_t DBG_RT_VIEW_COMMON_HZB_DESCRIPTOR_SLOT = 1;
static constexpr size_t DBG_RT_VIEW_GBUFFER_0_DESCRIPTOR_SLOT = 2;
static constexpr size_t DBG_RT_VIEW_GBUFFER_1_DESCRIPTOR_SLOT = 3;
static constexpr size_t DBG_RT_VIEW_GBUFFER_2_DESCRIPTOR_SLOT = 4;
static constexpr size_t DBG_RT_VIEW_GBUFFER_3_DESCRIPTOR_SLOT = 5;
static constexpr size_t DBG_RT_VIEW_IRRADIANCE_MAP_DESCRIPTOR_SLOT = 6;
static constexpr size_t DBG_RT_VIEW_PREFILTERED_ENV_MAP_DESCRIPTOR_SLOT = 7;
static constexpr size_t DBG_RT_VIEW_BRDF_LUT_DESCRIPTOR_SLOT = 8;
static constexpr size_t DBG_RT_VIEW_SKYBOX_DESCRIPTOR_SLOT = 9;
static constexpr size_t DBG_RT_VIEW_CSM_DESCRIPTOR_SLOT = 10;
static constexpr size_t DBG_RT_VIEW_COLOR_16F_DESCRIPTOR_SLOT = 11;


static constexpr uint32_t CSM_CASCADE_RT_SIZE = 2048;

static constexpr uint32_t GEOM_CULLING_PHASES_COUNT = 2;

static constexpr uint32_t COMMON_MATERIAL_TEXTURES_COUNT = 128;

static constexpr uint32_t MAX_DBG_LINE_COUNT = 16384;
static constexpr uint32_t MAX_DBG_TRIANGLE_COUNT = 2048;

static constexpr uint32_t DBG_LINE_VERTEX_COUNT = 2;
static constexpr uint32_t DBG_TRIANGLE_VERTEX_COUNT = 3; 

static constexpr uint32_t DBG_LINE_VERTEX_DATA_SIZE_UI = 2;
static constexpr uint32_t DBG_TRIANGLE_VERTEX_DATA_SIZE_UI = 2;

static constexpr uint32_t DBG_LINE_VERTEX_BUFFER_SIZE_UI = MAX_DBG_LINE_COUNT * DBG_LINE_VERTEX_COUNT * DBG_LINE_VERTEX_DATA_SIZE_UI;
static constexpr uint32_t DBG_LINE_VERTEX_BUFFER_SIZE = DBG_LINE_VERTEX_BUFFER_SIZE_UI * sizeof(glm::uint);
static constexpr uint32_t DBG_TRIANGLE_VERTEX_BUFFER_SIZE_UI = MAX_DBG_TRIANGLE_COUNT * DBG_TRIANGLE_VERTEX_COUNT * DBG_TRIANGLE_VERTEX_DATA_SIZE_UI;
static constexpr uint32_t DBG_TRIANGLE_VERTEX_BUFFER_SIZE = DBG_TRIANGLE_VERTEX_BUFFER_SIZE_UI * sizeof(glm::uint);

static constexpr size_t GBUFFER_RT_COUNT = 4;
static constexpr size_t CUBEMAP_FACE_COUNT = 6;

static constexpr size_t STAGING_BUFFER_SIZE  = 256 * 1024 * 1024; // 256 MB

static constexpr glm::uint  COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT = 10;
static constexpr float      COMMON_PREFILTERED_ENV_MAP_MIP_ROUGHNESS_DELTA = 1.f / (COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1);

static constexpr glm::uvec2 COMMON_IRRADIANCE_MAP_SIZE       = glm::uvec2(32);
static constexpr glm::uvec2 COMMON_PREFILTERED_ENV_MAP_SIZE  = glm::uvec2(glm::uint(1) << (COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1));
static constexpr glm::uvec2 COMMON_BRDF_INTEGRATION_LUT_SIZE = glm::uvec2(512);

static constexpr uint32_t HZB_MAX_MIP_COUNT = 12;

static constexpr uint32_t GEOM_CULLING_CS_GROUP_SIZE = 1024;
static constexpr uint32_t GEOM_BATCH_CS_GROUP_SIZE = 1024;
static constexpr uint32_t GEOM_DRAW_CMD_GEN_CS_GROUP_SIZE = 512;

static constexpr uint32_t HZB_BUILD_CS_GROUP_SIZE = 16;

static constexpr uint32_t DESC_SET_PER_FRAME = 0;
static constexpr uint32_t DESC_SET_PER_DRAW = 1;
static constexpr uint32_t DESC_SET_TOTAL_COUNT = 2;

static constexpr float LOD_SIMPLIFICATION_COEF = 0.50f; // Means that the next LOD should has on 50% less indices than current
static constexpr float LOD_SIMPLIFICATION_ERROR = 0.005f;
static constexpr size_t MAX_GEOM_LOD_COUNT = 8;


static constexpr const char* APP_NAME = "Vulkan Demo";

#if defined(ENG_BUILD_DEBUG)
    constexpr const char* APP_BUILD_TYPE_STR = "DEBUG";
#elif defined(ENG_BUILD_PROFILE)
    constexpr const char* APP_BUILD_TYPE_STR = "PROFILE";
#else
    constexpr const char* APP_BUILD_TYPE_STR = "RELEASE";
#endif  

static constexpr bool VSYNC_ENABLED = false;

static constexpr float CAMERA_SPEED = 0.01f;
static constexpr float CAMERA_ZNEAR = 0.01f;
static constexpr float CAMERA_ZFAR = 1'000.f;

static const glm::float3 SUN_LIGHT_DIR = glm::normalize(1.5f * M3D_AXIS_X - M3D_AXIS_Y - M3D_AXIS_Z);
static constexpr float SUN_DISTANCE = 100.f;

static constexpr std::array CSM_CASCADE_DISTANCES = { 40.f, 120.f, 250.f };

static constexpr std::array CSM_CASCADE_COLORS = {
    glm::float4(1.f, 0.f, 0.f, 0.45f),
    glm::float4(0.f, 1.f, 0.f, 0.45f),
    glm::float4(0.f, 0.f, 1.f, 0.45f),
};

static_assert(std::size(CSM_CASCADE_DISTANCES) == COMMON_CSM_CASCADE_COUNT);
static_assert(std::size(CSM_CASCADE_COLORS) == COMMON_CSM_CASCADE_COUNT);


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

static vkn::Buffer s_commonStagingBuffer;

static std::array<vkn::DescriptorSetLayout, PASS_ID_COUNT> s_descSetLayouts;

static std::array<vkn::PSOLayout, PASS_ID_COUNT> s_PSOLayouts;
static std::array<vkn::PSO,       PASS_ID_COUNT> s_PSOs;

static vkn::DescriptorBuffer s_descriptorBuffer;

static std::array<vkn::Buffer, COMMON_GEOM_STREAM_COUNT> s_geomStreamBuffers;
static vkn::Buffer s_geomIndexBuffer;

static vkn::Buffer s_commonConstBuffer;
static vkn::Buffer s_commonDbgConstBuffer;

static vkn::Buffer s_commonMeshLODBuffer;
static vkn::Buffer s_commonMeshBuffer;
static vkn::Buffer s_commonMaterialBuffer;
static vkn::Buffer s_commonInstBuffer;

static vkn::Buffer s_geomInstVisFlagsBuffer;

static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, GEOM_CULLING_PHASES_COUNT> s_visGeomIDQueueBuffers;
static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, GEOM_CULLING_PHASES_COUNT> s_visGeomIDQueueSizeBuffers;

static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, GEOM_CULLING_PHASES_COUNT> s_geomBatchQueueBuffers;
static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, GEOM_CULLING_PHASES_COUNT> s_geomBatchQueueSizeBuffers;

static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, GEOM_CULLING_PHASES_COUNT> s_sortedVisGeomIDQueueBuffers;
static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, GEOM_CULLING_PHASES_COUNT> s_sortedVisGeomIDQueueSizeBuffers;

static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, GEOM_CULLING_PHASES_COUNT> s_geomBatchDispatchCmdBuffers;
static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, GEOM_CULLING_PHASES_COUNT> s_geomDrawCmdGenDispatchCmdBuffers;

// We have two phase occlusion culling so basically we build two sets of draw commands and two sets of
// appropriate visible instance IDs (one for HZB generation and one for remaining visible instances). 
// We weant to render GBUFFER with one pass so we need to merge draw commands and IDs buffers into one
static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, GEOM_CULLING_PHASES_COUNT> s_geomDrawCmdQueueBuffers;


// CSM Data
static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, COMMON_CSM_CASCADE_COUNT> s_csmVisGeomIDQueueBuffers;
static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, COMMON_CSM_CASCADE_COUNT> s_csmVisGeomIDQueueSizeBuffers;

static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, COMMON_CSM_CASCADE_COUNT> s_csmGeomBatchQueueBuffers;
static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, COMMON_CSM_CASCADE_COUNT> s_csmGeomBatchQueueSizeBuffers;

static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, COMMON_CSM_CASCADE_COUNT> s_csmSortedVisGeomIDQueueBuffers;
static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, COMMON_CSM_CASCADE_COUNT> s_csmSortedVisGeomIDQueueSizeBuffers;

static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, COMMON_CSM_CASCADE_COUNT> s_csmGeomBatchDispatchCmdBuffers;
static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, COMMON_CSM_CASCADE_COUNT> s_csmGeomDrawCmdGenDispatchCmdBuffers;

static std::array<std::array<vkn::Buffer, GEOM_QUEUE_COUNT>, COMMON_CSM_CASCADE_COUNT> s_csmGeomDrawCmdQueueBuffers;

static vkn::Texture                                           s_csmRT;
static vkn::TextureView                                       s_csmRTViewArray;
static std::array<vkn::TextureView, COMMON_CSM_CASCADE_COUNT> s_csmRTViews;


static std::vector<vkn::Texture>     s_commonMaterialTextures;
static std::vector<vkn::TextureView> s_commonMaterialTextureViews;

static std::vector<vkn::Sampler> s_commonSamplers;

static std::array<std::vector<uint32_t>, COMMON_GEOM_STREAM_COUNT> s_cpuGeomStreamBuffers;
static std::vector<IndexType> s_cpuGeomIndexBuffer;

static std::vector<TextureLoadData> s_cpuTexturesData;

static std::vector<COMMON_MESH_LOD> s_cpuMeshLODData;
static std::vector<COMMON_MESH>     s_cpuMeshData;
static std::vector<COMMON_MATERIAL> s_cpuMaterialData;
static std::vector<COMMON_INST>     s_cpuInstData;


static std::vector<DBG_LINE_DATA>     s_dbgLineDataCPU;
static std::vector<DBG_TRIANGLE_DATA> s_dbgTriangleDataCPU;

static std::vector<glm::uint> s_dbgLineVertexDataCPU;
static std::vector<glm::uint> s_dbgTriangleVertexDataCPU;

static vkn::Buffer s_dbgLineDataGPU;
static vkn::Buffer s_dbgTriangleDataGPU;
static vkn::Buffer s_dbgLineVertexDataGPU;
static vkn::Buffer s_dbgTriangleVertexDataGPU;


static std::array<vkn::Texture, COMMON_DBG_TEX_IDX_COUNT>     s_commonDbgTextures;
static std::array<vkn::TextureView, COMMON_DBG_TEX_IDX_COUNT> s_commonDbgTextureViews;

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
static vkn::TextureView s_depthRTColorView;

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

static eng::Camera s_mainCamera;
static glm::float3 s_mainCameraVel = ZEROF3;
static bool s_mainCameraLoaded = false;

static std::array<eng::Camera, COMMON_CSM_CASCADE_COUNT> s_csmCameras;

static glm::float4x4 s_fixedCamCullViewProjMatr;
static glm::float4x4 s_fixedCamCullInvViewProjMatr;
static math::Frustum s_fixedCamCullFrustum;

static std::array<glm::float4x4, COMMON_CSM_CASCADE_COUNT> s_fixedCamCsmInvViewProjMatr;


static DBG_RT_VIEW_TYPE s_dbgOutputRTType = DBG_RT_VIEW_TYPE_NONE;
static float s_dbgDepthOutputRTZNear = 0.01f;
static float s_dbgDepthOutputRTZFar = 100.0f;

static int32_t s_dbgOutputRTMip = 0;
static int32_t s_dbgOutputRTFace = 0;
static int32_t s_dbgOutputRTCascadeIndex = 0;

static uint32_t s_nextImageIdx = 0;

static int32_t s_forcedGeomLOD = -1;

static size_t s_frameNumber = 0;
static float s_frameTime = M3D_EPS;
static bool s_swapchainRecreateRequired = false;
static bool s_flyCameraMode = false;
static bool s_cullingTestMode = false;
static bool s_csmTestMode = false;
static bool s_geomWireframeMode = false;

static bool s_skipRender = false;

#ifdef ENG_DEBUG_UI_ENABLED
    static bool s_useMeshCulling = true;
    static bool s_useMeshFrustumCulling = true;
    static bool s_useMeshHZBCulling = true;
    static bool s_useDepthPass = true;
    static bool s_useIndirectLighting = false;
    static bool s_drawInstAABBs = false;

    // Uses for debug purposes during CPU frustum culling
    static size_t s_dbgDrawnOpaqueMeshCount = 0;
    static size_t s_dbgDrawnAkillMeshCount = 0;
    static size_t s_dbgDrawnTranspMeshCount = 0;

    static TonemapPreset s_tonemappingPreset = TonemapPreset::ACES;
#else
    static constexpr bool s_useMeshCulling = true;
    static constexpr bool s_useMeshFrustumCulling = true;
    static constexpr bool s_useMeshHZBCulling = true;
    static constexpr bool s_useDepthPass = true;
    static constexpr bool s_useIndirectLighting = false;
    static constexpr bool s_drawInstAABBs = false;

    static constexpr TonemapPreset s_tonemappingPreset = TonemapPreset::ACES;
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


static bool IsInstFrustumVisible(const COMMON_INST& inst)
{
    ENG_PROFILE_TRANSIENT_SCOPED_MARKER_C("CPU_Is_Inst_Visible", eng::ProfileColor::Purple1);

    const math::Frustum& frustum = s_mainCamera.GetFrustum();

    return frustum.IsIntersect(inst.GetAABB_WCS()); 
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


static bool IsDebugLinesBufferFull()
{
    return s_dbgLineDataCPU.size() == MAX_DBG_LINE_COUNT;
}


static bool IsDebugTrianglesBufferFull()
{
    return s_dbgTriangleDataCPU.size() == MAX_DBG_TRIANGLE_COUNT;
}


static void RenderDebugLine(const glm::float3& wPos0, const glm::float3& wPos1, const glm::float4& color)
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    if (IsDebugLinesBufferFull()) {
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
    if (IsDebugTrianglesBufferFull()) {
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
static void RenderDebugFrustumInternal(const Func& func, const glm::float4x4& invViewProj, const glm::float4& color)
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

    glm::float4 bln = invViewProj * glm::float4(-1.f, BOTTOM_NDC_Y, NEAR_NDC_Z, 1.f);
    bln /= bln.w;
    
    glm::float4 brn = invViewProj * glm::float4(1.f, BOTTOM_NDC_Y, NEAR_NDC_Z, 1.f);
    brn /= brn.w;

    glm::float4 urn = invViewProj * glm::float4(1.f, TOP_NDC_Y, NEAR_NDC_Z, 1.f);
    urn /= urn.w;

    glm::float4 uln = invViewProj * glm::float4(-1.f, TOP_NDC_Y, NEAR_NDC_Z, 1.f);
    uln /= uln.w;

    glm::float4 blf = invViewProj * glm::float4(-1.f, BOTTOM_NDC_Y, FAR_NDC_Z, 1.f);
    blf /= blf.w;

    glm::float4 brf = invViewProj * glm::float4(1.f, BOTTOM_NDC_Y, FAR_NDC_Z, 1.f);
    brf /= brf.w;

    glm::float4 urf = invViewProj * glm::float4(1.f, TOP_NDC_Y, FAR_NDC_Z, 1.f);
    urf /= urf.w;

    glm::float4 ulf = invViewProj * glm::float4(-1.f, TOP_NDC_Y, FAR_NDC_Z, 1.f);
    ulf /= ulf.w;

    func(bln, brn, urn, uln, color);
    func(brn, brf, urf, urn, color);
    func(brf, blf, ulf, urf, color);
    func(blf, bln, uln, ulf, color);
    func(uln, urn, urf, ulf, color);
    func(blf, brf, brn, bln, color);
#endif
}


static void RenderDebugFrustumWired(const glm::float4x4& invViewProj, const glm::float4& color)
{
    RenderDebugFrustumInternal(RenderDebugQuadWire, invViewProj, color);
}


static void RenderDebugFrustumFilled(const glm::float4x4& invViewProj, const glm::float4& color)
{
    RenderDebugFrustumInternal(RenderDebugQuadFilled, invViewProj, color);
}


static bool IsAKillMaterial(const COMMON_MATERIAL& material)
{
    return (material.FLAGS & (uint32_t)COMMON_MATERIAL_FLAGS::ALPHA_KILL) != 0;
}


static bool IsOpaqueMaterial(const COMMON_MATERIAL& material)
{
    return !IsAKillMaterial(material);
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


static uint32_t CSMGetBufferIndex(uint32_t cascade, GEOM_QUEUE queue)
{
    return glm::clamp(cascade, 0u, COMMON_CSM_CASCADE_COUNT - 1) * GEOM_QUEUE_COUNT + queue;
}


static FRUSTUM CopyCPUFrustumToGPU(const math::Frustum& cpuFrustum)
{
    FRUSTUM gpuFrustum = {};

    for (size_t i = 0; i <  M3D_FRUSTUM_PLANE_COUNT; ++i) {
        const math::Plane& srcPlane = cpuFrustum.GetPlane(i);
        
        gpuFrustum.planes[i].normal = srcPlane.normal;
        gpuFrustum.planes[i].distance = srcPlane.distance;
    }

    return gpuFrustum;
}


static void SetWireframeMode(vkn::CmdBuffer& cmdBuffer, bool isWireframed)
{
    cmdBuffer.CmdSetPolygonMode(isWireframed ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL);
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


static void InitWindow()
{
    eng::WindowInitInfo wndInitInfo = {};
    wndInitInfo.pTitle = APP_NAME;
    wndInitInfo.width = 1280;
    wndInitInfo.height = 720;
    wndInitInfo.isVisible = false;

    s_pWnd = std::make_unique<eng::Win32Window>(wndInitInfo);
    ENG_ASSERT(s_pWnd && s_pWnd->IsCreated());
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


static void CreateVkSurface()
{
    vkn::SurfaceCreateInfo surfCreateInfo = {};
    surfCreateInfo.pInstance = &s_vkInstance;
    surfCreateInfo.pWndHandle = s_pWnd->GetNativeHandle();

    s_vkSurface.Create(surfCreateInfo);
    CORE_ASSERT(s_vkSurface.IsCreated());
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


static void CreateVkMemoryAllocator()
{
    vkn::AllocatorCreateInfo vkAllocatorCreateInfo = {}; 
    vkAllocatorCreateInfo.pDevice = &s_vkDevice;
    // RenderDoc doesn't work with buffer device address if you use VMA :(
    vkAllocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    s_vkAllocator.Create(vkAllocatorCreateInfo);
    CORE_ASSERT(s_vkAllocator.IsCreated());
}


static void CreateCommonCmdPool()
{
    vkn::CmdPoolCreateInfo cmdPoolCreateInfo = {};
    cmdPoolCreateInfo.pDevice = &s_vkDevice;
    cmdPoolCreateInfo.queueFamilyIndex = s_vkDevice.GetQueue().GetFamilyIndex();
    cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdPoolCreateInfo.size = 2;
    
    s_commonCmdPool.Create(cmdPoolCreateInfo);
    s_vkDevice.SetObjDebugName(s_commonCmdPool, "COMMON_CMD_POOL");
}


static void CreateImmediateSubmitObjects()
{
    s_pImmediateSubmitCmdBuffer = s_commonCmdPool.AllocCmdBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    s_vkDevice.SetObjDebugName(*s_pImmediateSubmitCmdBuffer, "IMMEDIATE_CMD_BUFFER");

    s_immediateSubmitFinishedFence.Create(&s_vkDevice);
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
    physDeviceFeturesReq.dynamicPolygonMode = true;
    physDeviceFeturesReq.shaderInt64 = true;

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
        VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
    };

    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extendedDynStateFeatures = {};
    extendedDynStateFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
    extendedDynStateFeatures.extendedDynamicState3PolygonMode = VK_TRUE;

    VkPhysicalDeviceDescriptorBufferFeaturesEXT descBuffFeatures = {};
    descBuffFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    descBuffFeatures.pNext = &extendedDynStateFeatures;
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
    features2.features.fillModeNonSolid = VK_TRUE;
    features2.features.shaderInt64 = VK_TRUE;

    vkn::DeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.pPhysDevice = &s_vkPhysDevice;
    deviceCreateInfo.pSurface = &s_vkSurface;
    deviceCreateInfo.queuePriority = 1.f;
    deviceCreateInfo.extensions = deviceExtensions;
    deviceCreateInfo.pFeatures2 = &features2;

    s_vkDevice.Create(deviceCreateInfo);
    CORE_ASSERT(s_vkDevice.IsCreated());

    s_vkDevice.SetObjDebugName(s_vkInstance, "VK_INSTANCE");
    s_vkDevice.SetObjDebugName(s_vkPhysDevice, "VK_PHYS_DEVICE");
    s_vkDevice.SetObjDebugName(s_vkDevice, "VK_DEVICE");
}


static void CreateCommonStagingBuffer()
{
    vkn::AllocationInfo stagingBufAllocInfo = {};
    stagingBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    stagingBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

    vkn::BufferCreateInfo stagingBufCreateInfo = {};
    stagingBufCreateInfo.pDevice = &s_vkDevice;
    stagingBufCreateInfo.size = STAGING_BUFFER_SIZE;
    stagingBufCreateInfo.usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT;
    stagingBufCreateInfo.pAllocInfo = &stagingBufAllocInfo;

    s_commonStagingBuffer.Create(stagingBufCreateInfo);
    s_vkDevice.SetObjDebugName(s_commonStagingBuffer, "COMMON_STAGING_BUFFER");
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

    gbuffRT0.Create(rtCreateInfo);
    s_vkDevice.SetObjDebugName(gbuffRT0, "COMMON_GBUFFER_0");

    gbuffRT0View.Create(gbuffRT0, mapping, subresourceRange);
    s_vkDevice.SetObjDebugName(gbuffRT0View, "COMMON_GBUFFER_0_VIEW");


    vkn::Texture& gbuffRT1 = s_gbufferRTs[1];
    vkn::TextureView& gbuffRT1View = s_gbufferRTViews[1];

    rtCreateInfo.format = VK_FORMAT_R16G16B16A16_SNORM;
    rtCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    gbuffRT1.Create(rtCreateInfo);
    s_vkDevice.SetObjDebugName(gbuffRT1, "COMMON_GBUFFER_1");

    gbuffRT1View.Create(gbuffRT1, mapping, subresourceRange);
    s_vkDevice.SetObjDebugName(gbuffRT1View, "COMMON_GBUFFER_1_VIEW");


    vkn::Texture& gbuffRT2 = s_gbufferRTs[2];
    vkn::TextureView& gbuffRT2View = s_gbufferRTViews[2];

    rtCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    rtCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    gbuffRT2.Create(rtCreateInfo);
    s_vkDevice.SetObjDebugName(gbuffRT2, "COMMON_GBUFFER_2");

    gbuffRT2View.Create(gbuffRT2, mapping, subresourceRange);
    s_vkDevice.SetObjDebugName(gbuffRT2View, "COMMON_GBUFFER_2_VIEW");


    vkn::Texture& gbuffRT3 = s_gbufferRTs[3];
    vkn::TextureView& gbuffRT3View = s_gbufferRTViews[3];

    rtCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    rtCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    gbuffRT3.Create(rtCreateInfo);
    s_vkDevice.SetObjDebugName(gbuffRT3, "COMMON_GBUFFER_3");

    gbuffRT3View.Create(gbuffRT3, mapping, subresourceRange);
    s_vkDevice.SetObjDebugName(gbuffRT3View, "COMMON_GBUFFER_3_VIEW");
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

    s_colorRT8U.Create(rtCreateInfo);
    s_vkDevice.SetObjDebugName(s_colorRT8U, "COMMON_COLOR_RT_U8");

    s_colorRTView8U.Create(s_colorRT8U, mapping, subresourceRange);
    s_vkDevice.SetObjDebugName(s_colorRTView8U, "COMMON_COLOR_RT_VIEW_U8");


    rtCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    rtCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    s_colorRT16F.Create(rtCreateInfo);
    s_vkDevice.SetObjDebugName(s_colorRT16F, "COMMON_COLOR_RT_16F");

    s_colorRTView16F.Create(s_colorRT16F, mapping, subresourceRange);
    s_vkDevice.SetObjDebugName(s_colorRTView16F, "COMMON_COLOR_RT_VIEW_16F");


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

    s_depthRT.Create(rtCreateInfo);
    s_vkDevice.SetObjDebugName(s_depthRT, "COMMON_DEPTH_RT");
    
    s_depthRTView.Create(s_depthRT, mapping, subresourceRange);
    s_vkDevice.SetObjDebugName(s_depthRTView, "COMMON_DEPTH_RT_VIEW");


    vkn::TextureViewCreateInfo depthColorViewCreateInfo = {};
    depthColorViewCreateInfo.pOwner = &s_depthRT;
    depthColorViewCreateInfo.type = s_depthRTView.GetType();
    depthColorViewCreateInfo.format = VK_FORMAT_D32_SFLOAT;
    depthColorViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    depthColorViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthColorViewCreateInfo.subresourceRange.baseMipLevel = 0;
    depthColorViewCreateInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    depthColorViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    depthColorViewCreateInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    s_depthRTColorView.Create(depthColorViewCreateInfo);
    s_vkDevice.SetObjDebugName(s_depthRTColorView, "COMMON_DEPTH_RT_COLOR_VIEW");
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

    s_HZB.Create(rtCreateInfo);
    s_vkDevice.SetObjDebugName(s_HZB, "HZB");

    VkComponentMapping mapping = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    s_HZBView.Create(s_HZB, mapping, subresourceRange);
    s_vkDevice.SetObjDebugName(s_HZBView, "HZB_VIEW");
    
    s_HZBMipViews.resize(mipsCount);

    subresourceRange.levelCount = 1;
    
    for (uint32_t mip = 0; mip < mipsCount; ++mip) {
        subresourceRange.baseMipLevel = mip;

        s_HZBMipViews[mip].Create(s_HZB, mapping, subresourceRange);
        s_vkDevice.SetObjDebugName(s_HZBMipViews[mip], "HZB_MIP_%u", mip);
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

    s_depthRTColorView.Destroy();
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

    s_skyboxTexture.Create(createInfo);
    s_vkDevice.SetObjDebugName(s_skyboxTexture, "COMMON_SKY_BOX");
    
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

    s_skyboxTextureView.Create(viewCreateInfo);
    s_vkDevice.SetObjDebugName(s_skyboxTextureView, "COMMON_SKY_BOX_VIEW");

    for (size_t i = 0; i < CUBEMAP_FACE_COUNT; ++i) {
        const TextureLoadData& loadData = faceLoadDatas[i];

        void* pData = s_commonStagingBuffer.Map();
        memcpy(pData, loadData.GetData(), loadData.GetMemorySize());
        s_commonStagingBuffer.Unmap();

        ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
            cmdBuffer
                .BeginBarrierList()
                    .AddTextureBarrier(s_skyboxTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT, 
                        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1)
                .Push();

            const uint32_t faceIdx = i;

            vkn::BufferToTextureCopyInfo copyInfo = {};
            copyInfo.texSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyInfo.texSubresource.mipLevel = 0;
            copyInfo.texSubresource.baseArrayLayer = faceIdx;
            copyInfo.texSubresource.layerCount = 1;
            copyInfo.texExtent = s_skyboxTexture.GetSize();

            cmdBuffer.CmdCopyBuffer(s_commonStagingBuffer, s_skyboxTexture, copyInfo);
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


static void CreateSyncObjects()
{
    const size_t swapchainImageCount = s_vkSwapchain.GetTextureCount();

    s_renderFinishedSemaphores.resize(swapchainImageCount);
    for (size_t i = 0; i < swapchainImageCount; ++i) {
        s_renderFinishedSemaphores[i].Create(&s_vkDevice);
        s_vkDevice.SetObjDebugName(s_renderFinishedSemaphores[i], "RND_FINISH_SEMAPHORE_%zu", i);
    }
    s_presentFinishedSemaphore.Create(&s_vkDevice);
    s_vkDevice.SetObjDebugName(s_presentFinishedSemaphore, "PRESENT_FINISH_SEMAPHORE");

    s_renderFinishedFence.Create(&s_vkDevice);
    s_vkDevice.SetObjDebugName(s_renderFinishedFence, "RND_FINISH_FENCE");
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
    
        s_irradianceMapTexture.Create(createInfo);
        s_vkDevice.SetObjDebugName(s_irradianceMapTexture, "COMMON_IRRADIANCE_MAP");
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
        
        s_prefilteredEnvMapTexture.Create(createInfo);
        s_vkDevice.SetObjDebugName(s_prefilteredEnvMapTexture, "COMMON_PREFILTERED_ENV_MAP");
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

        s_brdfLUTTexture.Create(createInfo);
        s_vkDevice.SetObjDebugName(s_brdfLUTTexture, "COMMON_BRDF_LUT");
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
    
        s_irradianceMapTextureView.Create(viewCreateInfo);
        s_vkDevice.SetObjDebugName(s_irradianceMapTextureView, "COMMON_IRRADIANCE_MAP_VIEW");
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
        
        s_irradianceMapTextureViewRW.Create(viewCreateInfo);
        s_vkDevice.SetObjDebugName(s_irradianceMapTextureViewRW, "COMMON_IRRADIANCE_MAP_VIEW_RW");
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
        
        s_prefilteredEnvMapTextureView.Create(viewCreateInfo);
        s_vkDevice.SetObjDebugName(s_prefilteredEnvMapTextureView, "COMMON_PREFILTERED_ENV_MAP_VIEW");
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
    
                vkn::TextureView& mipView = s_prefilteredEnvMapTextureViewRWs[layer * COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT + mip];
                mipView.Create(viewCreateInfo);
                s_vkDevice.SetObjDebugName(mipView, "COMMON_PREFILTERED_ENV_MAP_VIEW_RW_LAYER_%zu_MIP_%zu", layer, mip);
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
        
        s_brdfLUTTextureView.Create(viewCreateInfo);
        s_vkDevice.SetObjDebugName(s_brdfLUTTextureView, "COMMON_BRDF_LUT_VIEW");
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
        
        s_brdfLUTTextureViewRW.Create(viewCreateInfo);
        s_vkDevice.SetObjDebugName(s_brdfLUTTextureViewRW, "COMMON_BRDF_LUT_VIEW_RW");
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
    
        s_dbgLineDataGPU.Create(&s_vkDevice, MAX_DBG_LINE_COUNT * sizeof(DBG_LINE_DATA), VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT, allocInfo);
        s_vkDevice.SetObjDebugName(s_dbgLineDataGPU, "DBG_DRAW_LINE_DATA_BUFFER");
    }

    {
        s_dbgLineVertexDataCPU.reserve(DBG_LINE_VERTEX_BUFFER_SIZE_UI);

        vkn::AllocationInfo allocInfo = {};
        allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    
        s_dbgLineVertexDataGPU.Create(&s_vkDevice, DBG_LINE_VERTEX_BUFFER_SIZE, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT, allocInfo);
        s_vkDevice.SetObjDebugName(s_dbgLineVertexDataGPU, "DBG_DRAW_LINE_VERT_BUFFER");
    }

    {
        s_dbgTriangleDataCPU.reserve(MAX_DBG_TRIANGLE_COUNT);
    
        vkn::AllocationInfo allocInfo = {};
        allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    
        s_dbgTriangleDataGPU.Create(&s_vkDevice, MAX_DBG_TRIANGLE_COUNT * sizeof(DBG_TRIANGLE_DATA), VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT, allocInfo);
        s_vkDevice.SetObjDebugName(s_dbgTriangleDataGPU, "DBG_DRAW_TRIANGLE_DATA_BUFFER");
    }

    {
        s_dbgTriangleVertexDataCPU.reserve(DBG_TRIANGLE_VERTEX_BUFFER_SIZE_UI);

        vkn::AllocationInfo allocInfo = {};
        allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    
        s_dbgTriangleVertexDataGPU.Create(&s_vkDevice, DBG_TRIANGLE_VERTEX_BUFFER_SIZE, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT, allocInfo);
        s_vkDevice.SetObjDebugName(s_dbgTriangleVertexDataGPU, "DBG_DRAW_TRIANGLE_VERT_BUFFER");
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
        vkn::DescriptorInfo::Create(COMMON_SAMPLERS_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLER, SAMPLER_IDX_COUNT, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_DBG_TEXTURES_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, COMMON_DBG_TEX_IDX_COUNT, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_CB_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_DBG_CB_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_GEOM_STREAMS_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, COMMON_GEOM_STREAM_COUNT, VK_SHADER_STAGE_VERTEX_BIT),
        vkn::DescriptorInfo::Create(COMMON_MESH_LOD_BUFFER_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_MESH_BUFFER_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_MATERIALS_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_MTL_TEXTURES_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, COMMON_MATERIAL_TEXTURES_COUNT, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(COMMON_INST_BUFFER_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_DEPTH_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_ALL),
        vkn::DescriptorInfo::Create(COMMON_HZB_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_ALL),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[PASS_ID_COMMON].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_COMMON], "COMMON_DESCRIPTOR_SET_LAYOUT");
}


static void CreateGeomCullingPhase1DescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(GEOM_CULL_VIS_INST_ID_QUEUES_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GEOM_QUEUE_COUNT, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(GEOM_CULL_VIS_INST_ID_QUEUE_SIZES_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GEOM_QUEUE_COUNT, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(GEOM_CULL_VIS_FLAGS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(GEOM_CULL_BATCH_DISPATCH_CMDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GEOM_QUEUE_COUNT, VK_SHADER_STAGE_COMPUTE_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[PASS_ID_GEOM_CULLING_PHASE_1].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_GEOM_CULLING_PHASE_1], "PREV_FRAME_OCCLUDERS_CULLING_DESCRIPTOR_SET_LAYOUT");
}


static void CreateGeomCullingPhase2DescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(GEOM_CULL_VIS_INST_ID_QUEUES_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GEOM_QUEUE_COUNT, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(GEOM_CULL_VIS_INST_ID_QUEUE_SIZES_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GEOM_QUEUE_COUNT, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(GEOM_CULL_VIS_FLAGS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(GEOM_CULL_BATCH_DISPATCH_CMDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GEOM_QUEUE_COUNT, VK_SHADER_STAGE_COMPUTE_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[PASS_ID_GEOM_CULLING_PHASE_2].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_GEOM_CULLING_PHASE_2], "THIS_FRAME_GEOMETRY_CULLING_DESCRIPTOR_SET_LAYOUT");
}


static void CreateGeomBatchingDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(GEOM_BATCH_VIS_INST_ID_QUEUE_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GEOM_QUEUE_COUNT, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(GEOM_BATCH_VIS_INST_ID_QUEUE_SIZE_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(GEOM_BATCH_BATCH_QUEUE_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GEOM_QUEUE_COUNT, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(GEOM_BATCH_BATCH_QUEUE_SIZE_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(GEOM_BATCH_SORTED_VIS_INST_ID_QUEUE_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GEOM_QUEUE_COUNT, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(GEOM_BATCH_SORTED_VIS_INST_ID_QUEUE_SIZE_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(GEOM_BATCH_DRAW_CMD_GEN_DISPATCH_CMD_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[PASS_ID_GEOM_BATCHING].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_GEOM_BATCHING], "GEOM_BATCHING_DESCRIPTOR_SET_LAYOUT");
}


static void CreateGeomDrawCmdGenDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(GEOM_DRAW_CMD_GEN_BATCH_QUEUE_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(GEOM_DRAW_CMD_GEN_BATCH_QUEUE_SIZE_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(GEOM_DRAW_CMD_GEN_CMD_QUEUE_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[PASS_ID_GEOM_DRAW_CMD_GEN].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_GEOM_DRAW_CMD_GEN], "GEOM_DRAW_CMD_GEN_DESCRIPTOR_SET_LAYOUT");
}


static void CreateZPassDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(ZPASS_INST_ID_QUEUE_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[PASS_ID_DEPTH].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_DEPTH], "ZPASS_DESCRIPTOR_SET_LAYOUT");
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

    s_descSetLayouts[PASS_ID_HZB_GEN].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_HZB_GEN], "HZB_GEN_DESCRIPTOR_SET_LAYOUT");
}


static void CreateCSMGeomCullingDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(CSM_VIS_INST_ID_QUEUES_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, CSM_BUFFER_COUNT, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(CSM_VIS_INST_ID_QUEUE_SIZES_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, CSM_BUFFER_COUNT, VK_SHADER_STAGE_COMPUTE_BIT),
        vkn::DescriptorInfo::Create(CSM_BATCH_DISPATCH_CMDS_UAV_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, CSM_BUFFER_COUNT, VK_SHADER_STAGE_COMPUTE_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[PASS_ID_CSM_GEOM_CULLING].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_CSM_GEOM_CULLING], "CSM_GEOM_CULLING_DESCRIPTOR_SET_LAYOUT");
}


static void CreateCSMRenderDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(CSM_INST_ID_QUEUE_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, CSM_BUFFER_COUNT, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[PASS_ID_CSM_RENDER].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_CSM_RENDER], "CSM_RENDER_SET_LAYOUT");
}


static void CreateGBufferDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(GBUFFER_INST_ID_QUEUE_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[PASS_ID_GBUFFER].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_GBUFFER], "GBUFFER_DESCRIPTOR_SET_LAYOUT");
}


static void CreateDeferredLightingDescriptorSetLayout()
{
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_GBUFFER_0_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_GBUFFER_1_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_GBUFFER_2_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_GBUFFER_3_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_DEPTH_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_IRRADIANCE_MAP_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_PREFILTERED_ENV_MAP_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_BRDF_LUT_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DEFERRED_LIGHTING_CSM_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[PASS_ID_DEFERRED_LIGHTING].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_DEFERRED_LIGHTING], "DEFERRED_LIGHTING_DESCRIPTOR_SET_LAYOUT");
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

    s_descSetLayouts[PASS_ID_POST_PROCESSING].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_POST_PROCESSING], "POST_PROCESSING_DESCRIPTOR_SET_LAYOUT");
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

    s_descSetLayouts[PASS_ID_BACKBUFFER].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_BACKBUFFER], "BACK_BUFFER_DESCRIPTOR_SET_LAYOUT");
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

    s_descSetLayouts[PASS_ID_SKYBOX].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_SKYBOX], "SKYBOX_DESCRIPTOR_SET_LAYOUT");
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

    s_descSetLayouts[PASS_ID_IRRADIANCE_MAP_GEN].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_IRRADIANCE_MAP_GEN], "IRRADIANCE_MAP_GEN_DESCRIPTOR_SET_LAYOUT");
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

    s_descSetLayouts[PASS_ID_PREFILT_ENV_MAP_GEN].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_PREFILT_ENV_MAP_GEN], "PREFILT_ENV_MAP_GEN_DESCRIPTOR_SET_LAYOUT");
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

    s_descSetLayouts[PASS_ID_BRDF_LUT_GEN].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_BRDF_LUT_GEN], "BRDF_INTEGRATION_LUT_GEN_DESCRIPTOR_SET_LAYOUT");
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

    s_descSetLayouts[PASS_ID_DBG_DRAW_LINES].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_DBG_DRAW_LINES], "DBG_DRAW_LINES_DESCRIPTOR_SET_LAYOUT");
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

    s_descSetLayouts[PASS_ID_DBG_DRAW_TRIANGLES].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_DBG_DRAW_TRIANGLES], "DBG_DRAW_TRIANGLES_DESCRIPTOR_SET_LAYOUT");
#endif
}


static void CreateDbgRTViewDescriptorSetLayout()
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    vkn::DescriptorSetLayoutCreateInfo createInfo = {};

    createInfo.pDevice = &s_vkDevice;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    // createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    std::array descriptors = {
        vkn::DescriptorInfo::Create(DBG_RT_VIEW_COMMON_DEPTH_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DBG_RT_VIEW_COMMON_HZB_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DBG_RT_VIEW_GBUFFER_0_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DBG_RT_VIEW_GBUFFER_1_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DBG_RT_VIEW_GBUFFER_2_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DBG_RT_VIEW_GBUFFER_3_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DBG_RT_VIEW_IRRADIANCE_MAP_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DBG_RT_VIEW_PREFILTERED_ENV_MAP_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DBG_RT_VIEW_BRDF_LUT_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DBG_RT_VIEW_SKYBOX_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DBG_RT_VIEW_CSM_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
        vkn::DescriptorInfo::Create(DBG_RT_VIEW_COLOR_16F_DESCRIPTOR_SLOT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    createInfo.descriptorInfos = descriptors;

    s_descSetLayouts[PASS_ID_DBG_RT_VIEW].Create(createInfo);
    s_vkDevice.SetObjDebugName(s_descSetLayouts[PASS_ID_DBG_RT_VIEW], "DBG_RT_VIEW_DESCRIPTOR_SET_LAYOUT");
#endif
}


static void CreateDescriptorBuffer()
{
    std::array<vkn::DescriptorSetLayout*, DESC_SET_ID_COUNT> layouts = {};
    
    layouts[DESC_SET_ID_COMMON] = &s_descSetLayouts[PASS_ID_COMMON];

    layouts[DESC_SET_ID_GEOM_CULLING_PHASE_1] = &s_descSetLayouts[PASS_ID_GEOM_CULLING_PHASE_1];
    layouts[DESC_SET_ID_GEOM_CULLING_PHASE_2] = &s_descSetLayouts[PASS_ID_GEOM_CULLING_PHASE_2];
    
    layouts[DESC_SET_ID_GEOM_BATCHING_OPAQUE_PHASE_1] = &s_descSetLayouts[PASS_ID_GEOM_BATCHING];
    layouts[DESC_SET_ID_GEOM_BATCHING_OPAQUE_PHASE_2] = &s_descSetLayouts[PASS_ID_GEOM_BATCHING];
    layouts[DESC_SET_ID_GEOM_BATCHING_AKILL_PHASE_1]  = &s_descSetLayouts[PASS_ID_GEOM_BATCHING];
    layouts[DESC_SET_ID_GEOM_BATCHING_AKILL_PHASE_2]  = &s_descSetLayouts[PASS_ID_GEOM_BATCHING];
    
    layouts[DESC_SET_ID_GEOM_DRAW_CMD_GEN_OPAQUE_PHASE_1] = &s_descSetLayouts[PASS_ID_GEOM_DRAW_CMD_GEN];
    layouts[DESC_SET_ID_GEOM_DRAW_CMD_GEN_OPAQUE_PHASE_2] = &s_descSetLayouts[PASS_ID_GEOM_DRAW_CMD_GEN];
    layouts[DESC_SET_ID_GEOM_DRAW_CMD_GEN_AKILL_PHASE_1]  = &s_descSetLayouts[PASS_ID_GEOM_DRAW_CMD_GEN];
    layouts[DESC_SET_ID_GEOM_DRAW_CMD_GEN_AKILL_PHASE_2]  = &s_descSetLayouts[PASS_ID_GEOM_DRAW_CMD_GEN];
    
    layouts[DESC_SET_ID_DEPTH_OPAQUE_PHASE_1] = &s_descSetLayouts[PASS_ID_DEPTH];
    layouts[DESC_SET_ID_DEPTH_OPAQUE_PHASE_2] = &s_descSetLayouts[PASS_ID_DEPTH];
    layouts[DESC_SET_ID_DEPTH_AKILL_PHASE_1]  = &s_descSetLayouts[PASS_ID_DEPTH];
    layouts[DESC_SET_ID_DEPTH_AKILL_PHASE_2]  = &s_descSetLayouts[PASS_ID_DEPTH];
    
    layouts[DESC_SET_ID_HZB_GEN] = &s_descSetLayouts[PASS_ID_HZB_GEN];

    layouts[DESC_SET_ID_CSM_GEOM_CULLING] = &s_descSetLayouts[PASS_ID_CSM_GEOM_CULLING],
    layouts[DESC_SET_ID_CSM_GEOM_BATCHING_OPAQUE_CASCADE_0] = &s_descSetLayouts[PASS_ID_GEOM_BATCHING],
    layouts[DESC_SET_ID_CSM_GEOM_BATCHING_OPAQUE_CASCADE_1] = &s_descSetLayouts[PASS_ID_GEOM_BATCHING],
    layouts[DESC_SET_ID_CSM_GEOM_BATCHING_OPAQUE_CASCADE_2] = &s_descSetLayouts[PASS_ID_GEOM_BATCHING],
    layouts[DESC_SET_ID_CSM_GEOM_BATCHING_AKILL_CASCADE_0]  = &s_descSetLayouts[PASS_ID_GEOM_BATCHING],
    layouts[DESC_SET_ID_CSM_GEOM_BATCHING_AKILL_CASCADE_1]  = &s_descSetLayouts[PASS_ID_GEOM_BATCHING],
    layouts[DESC_SET_ID_CSM_GEOM_BATCHING_AKILL_CASCADE_2]  = &s_descSetLayouts[PASS_ID_GEOM_BATCHING],
    layouts[DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_OPAQUE_CASCADE_0] = &s_descSetLayouts[PASS_ID_GEOM_DRAW_CMD_GEN],
    layouts[DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_OPAQUE_CASCADE_1] = &s_descSetLayouts[PASS_ID_GEOM_DRAW_CMD_GEN],
    layouts[DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_OPAQUE_CASCADE_2] = &s_descSetLayouts[PASS_ID_GEOM_DRAW_CMD_GEN],
    layouts[DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_AKILL_CASCADE_0]  = &s_descSetLayouts[PASS_ID_GEOM_DRAW_CMD_GEN],
    layouts[DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_AKILL_CASCADE_1]  = &s_descSetLayouts[PASS_ID_GEOM_DRAW_CMD_GEN],
    layouts[DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_AKILL_CASCADE_2]  = &s_descSetLayouts[PASS_ID_GEOM_DRAW_CMD_GEN],
    layouts[DESC_SET_ID_CSM_RENDER] = &s_descSetLayouts[PASS_ID_CSM_RENDER],

    layouts[DESC_SET_ID_GBUFFER_OPAQUE_PHASE_1] = &s_descSetLayouts[PASS_ID_GBUFFER];
    layouts[DESC_SET_ID_GBUFFER_OPAQUE_PHASE_2] = &s_descSetLayouts[PASS_ID_GBUFFER];
    layouts[DESC_SET_ID_GBUFFER_AKILL_PHASE_1]  = &s_descSetLayouts[PASS_ID_GBUFFER];
    layouts[DESC_SET_ID_GBUFFER_AKILL_PHASE_2]  = &s_descSetLayouts[PASS_ID_GBUFFER];
    
    layouts[DESC_SET_ID_DEFERRED_LIGHTING] = &s_descSetLayouts[PASS_ID_DEFERRED_LIGHTING];
    
    layouts[DESC_SET_ID_SKYBOX] = &s_descSetLayouts[PASS_ID_SKYBOX];
    
    layouts[DESC_SET_ID_POST_PROCESSING] = &s_descSetLayouts[PASS_ID_POST_PROCESSING];
    
    layouts[DESC_SET_ID_BACKBUFFER] = &s_descSetLayouts[PASS_ID_BACKBUFFER];
    
    layouts[DESC_SET_ID_IRRADIANCE_MAP_GEN]  = &s_descSetLayouts[PASS_ID_IRRADIANCE_MAP_GEN];
    layouts[DESC_SET_ID_BRDF_LUT_GEN]        = &s_descSetLayouts[PASS_ID_BRDF_LUT_GEN];
    layouts[DESC_SET_ID_PREFILT_ENV_MAP_GEN] = &s_descSetLayouts[PASS_ID_PREFILT_ENV_MAP_GEN];
    
#ifdef ENG_DEBUG_DRAW_ENABLED
    layouts[DESC_SET_ID_DBG_DRAW_LINES]     = &s_descSetLayouts[PASS_ID_DBG_DRAW_LINES];
    layouts[DESC_SET_ID_DBG_DRAW_TRIANGLES] = &s_descSetLayouts[PASS_ID_DBG_DRAW_TRIANGLES];
    layouts[DESC_SET_ID_DBG_RT_VIEW]        = &s_descSetLayouts[PASS_ID_DBG_RT_VIEW];
#endif

    for (size_t i = 0; i < layouts.size(); ++i) {
        CORE_ASSERT_MSG(layouts[i] && layouts[i]->IsCreated(), "Descriptor Set Layout %s is not created", DESC_SET_DBG_NAME[i]);
    }

    s_descriptorBuffer.Create(&s_vkDevice, layouts).SetDebugName("COMMON_DESCRIPTOR_BUFFER");
}


static void CreateDescriptorSets()
{
    CreateCommonDescriptorSetLayout();

    CreateGeomCullingPhase1DescriptorSetLayout();
    CreateGeomCullingPhase2DescriptorSetLayout();
    CreateGeomBatchingDescriptorSetLayout();
    CreateGeomDrawCmdGenDescriptorSetLayout();
    
    CreateZPassDescriptorSetLayout();
    
    CreateHZBGenDescriptorSetLayout();
    
    CreateCSMGeomCullingDescriptorSetLayout();
    CreateCSMRenderDescriptorSetLayout();

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
    CreateDbgRTViewDescriptorSetLayout();

    
    CreateDescriptorBuffer();
}


static void CreateGeomCullingPhase1PipelineLayout()
{
    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GEOM_CULLING_PER_DRAW_DATA) };

    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_GEOM_CULLING_PHASE_1];

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_GEOM_CULLING_PHASE_1];
    
    layout.Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1));
    s_vkDevice.SetObjDebugName(layout, "PREV_FRAME_OCCLUDERS_CULLING_PIPELINE_LAYOUT");
}


static void CreateGeomCullingPhase2PipelineLayout()
{
    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GEOM_CULLING_PER_DRAW_DATA) };

    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_GEOM_CULLING_PHASE_2];

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_GEOM_CULLING_PHASE_2];
    
    layout.Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1));
    s_vkDevice.SetObjDebugName(layout, "THIS_FRAME_GEOMETRY_CULLING_PIPELINE_LAYOUT");
}


static void CreateGeomBatchingPipelineLayout()
{
    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GEOM_BATCH_PER_DRAW_DATA) };

    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_GEOM_BATCHING];

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_GEOM_BATCHING];
    
    layout.Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1));
    s_vkDevice.SetObjDebugName(layout, "GEOM_BATCHING_PIPELINE_LAYOUT");
}


static void CreateGeomDrawCmdGenPipelineLayout()
{
    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GEOM_DRAW_CMD_GEN_PER_DRAW_DATA) };

    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_GEOM_DRAW_CMD_GEN];

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_GEOM_DRAW_CMD_GEN];
    
    layout.Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1));
    s_vkDevice.SetObjDebugName(layout, "GEOM_DRAW_CMD_GEN_PIPELINE_LAYOUT");
}


static void CreateZPassPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_DEPTH];

    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ZPASS_PER_DRAW_DATA) };

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_DEPTH];

    layout.Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1));
    s_vkDevice.SetObjDebugName(layout, "ZPASS_PIPELINE_LAYOUT");
}


static void CreateHZBGenPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_HZB_GEN];

    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(HZB_GEN_PER_DRAW_DATA) };

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_HZB_GEN];

    layout.Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1));
    s_vkDevice.SetObjDebugName(layout, "HZB_GEN_PIPELINE_LAYOUT");
}


static void CreateCSMGeomCullingPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_CSM_GEOM_CULLING];

    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CSM_PER_DRAW_DATA) };

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_CSM_GEOM_CULLING];
    
    layout.Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1));
    s_vkDevice.SetObjDebugName(layout, "CSM_GEOM_CULLING_PIPELINE_LAYOUT");
}


static void CreateCSMRenderPipelineLayout()
{   
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_CSM_RENDER];
    
    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(CSM_PER_DRAW_DATA) };
    
    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_CSM_RENDER];
    
    layout.Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1));
    s_vkDevice.SetObjDebugName(layout, "CSM_RENDER_PIPELINE_LAYOUT");
}


static void CreateGBufferPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_GBUFFER];
    
    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GBUFFER_PER_DRAW_DATA) };

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_GBUFFER];

    layout.Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1));
    s_vkDevice.SetObjDebugName(layout, "GBUFFER_PIPELINE_LAYOUT");
}


static void CreateDeferredLightingPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_DEFERRED_LIGHTING];

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_DEFERRED_LIGHTING];

    layout.Create(&s_vkDevice, layoutPtrs);
    s_vkDevice.SetObjDebugName(layout, "DEFERRED_LIGHTING_PIPELINE_LAYOUT");
}


static void CreatePostProcessingPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_POST_PROCESSING];

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_POST_PROCESSING];

    layout.Create(&s_vkDevice, layoutPtrs);
    s_vkDevice.SetObjDebugName(layout, "POST_PROCESSING_PIPELINE_LAYOUT");
}


static void CreateBackbufferPassPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_BACKBUFFER];

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_BACKBUFFER];

    layout.Create(&s_vkDevice, layoutPtrs);
    s_vkDevice.SetObjDebugName(layout, "BACKBUFFER_PIPELINE_LAYOUT");
}


static void CreateSkyboxPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_SKYBOX];

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_SKYBOX];

    layout.Create(&s_vkDevice, layoutPtrs);
    s_vkDevice.SetObjDebugName(layout, "SKYBOX_PIPELINE_LAYOUT");
}


static void CreateIrradianceMapGenPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_IRRADIANCE_MAP_GEN];

    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(IRRADIANCE_MAP_PER_DRAW_DATA) };

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_IRRADIANCE_MAP_GEN];

    layout.Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1));
    s_vkDevice.SetObjDebugName(layout, "IRRAD_MAP_GEN_PIPELINE_LAYOUT");
}


static void CreatePrefilteredEnvMapGenPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_PREFILT_ENV_MAP_GEN];

    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PREFILTERED_ENV_MAP_PER_DRAW_DATA) };

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_PREFILT_ENV_MAP_GEN];
    
    layout.Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1));
    s_vkDevice.SetObjDebugName(layout, "PREFILT_ENV_MAP_GET_PIPELINE_LAYOUT");
}


static void CreateBRDFIntegrationLUTGenPipelineLayout()
{
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_BRDF_LUT_GEN];

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_BRDF_LUT_GEN];

    layout.Create(&s_vkDevice, layoutPtrs);
    s_vkDevice.SetObjDebugName(layout, "GRDF_LUT_GEN_PIPELINE_LAYOUT");
}


static void CreateDbgDrawLinePipelineLayout()
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_DBG_DRAW_LINES];

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_DBG_DRAW_LINES];

    layout.Create(&s_vkDevice, layoutPtrs);
    s_vkDevice.SetObjDebugName(layout, "DBG_DRAW_LINE_PIPELINE_LAYOUT");
#endif
}


static void CreateDbgDrawTrianglePipelineLayout()
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_DBG_DRAW_TRIANGLES];

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_DBG_DRAW_TRIANGLES];

    layout.Create(&s_vkDevice, layoutPtrs);
    s_vkDevice.SetObjDebugName(layout, "DBG_DRAW_TRIANGLES_PIPELINE_LAYOUT");
#endif
}


static void CreateDbgRTViewPipelineLayout()
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    const vkn::DescriptorSetLayout* layoutPtrs[DESC_SET_TOTAL_COUNT] = {};
    layoutPtrs[DESC_SET_PER_FRAME] = &s_descSetLayouts[PASS_ID_COMMON];
    layoutPtrs[DESC_SET_PER_DRAW] = &s_descSetLayouts[PASS_ID_DBG_RT_VIEW];

    VkPushConstantRange pushConstRange = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DBG_RT_VIEW_PER_DRAW_DATA) };

    vkn::PSOLayout& layout = s_PSOLayouts[PASS_ID_DBG_RT_VIEW];

    layout.Create(&s_vkDevice, layoutPtrs, std::span(&pushConstRange, 1));
    s_vkDevice.SetObjDebugName(layout, "DBG_RT_VIEW_PIPELINE_LAYOUT");
#endif
}


static void CreateGeomCullingPhase1Pipeline(const fs::path& csPath)
{
    if (!LoadShaderSpirVCode(csPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", csPath.string().c_str());
    }
    
    vkn::Shader shader;
    shader.Create(&s_vkDevice, VK_SHADER_STAGE_COMPUTE_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(shader, "PREV_FRAME_OCCLUDERS_CULLING_COMPUTE_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_GEOM_CULLING_PHASE_1];

    pso = s_computePSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .SetShader(shader)
        .SetLayout(s_PSOLayouts[PASS_ID_GEOM_CULLING_PHASE_1])
        .Build();

    s_vkDevice.SetObjDebugName(pso, "PREV_FRAME_OCCLUDERS_CULLING_PSO");
}


static void CreateGeomCullingPhase2Pipeline(const fs::path& csPath)
{
    if (!LoadShaderSpirVCode(csPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", csPath.string().c_str());
    }
    
    vkn::Shader shader;
    shader.Create(&s_vkDevice, VK_SHADER_STAGE_COMPUTE_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(shader, "THIS_FRAME_GEOMETRY_CULLING_COMPUTE_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_GEOM_CULLING_PHASE_2];

    pso = s_computePSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .SetShader(shader)
        .SetLayout(s_PSOLayouts[PASS_ID_GEOM_CULLING_PHASE_2])
        .Build();

    s_vkDevice.SetObjDebugName(pso, "THIS_FRAME_GEOMETRY_CULLING_PSO");
}


static void CreateGeomBatchingPipeline(const fs::path& csPath)
{
    if (!LoadShaderSpirVCode(csPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", csPath.string().c_str());
    }
    
    vkn::Shader shader;
    shader.Create(&s_vkDevice, VK_SHADER_STAGE_COMPUTE_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(shader, "GEOM_BATCHING_COMPUTE_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_GEOM_BATCHING];

    pso = s_computePSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .SetShader(shader)
        .SetLayout(s_PSOLayouts[PASS_ID_GEOM_BATCHING])
        .Build();

    s_vkDevice.SetObjDebugName(pso, "GEOM_BATCHING_PSO");
}


static void CreateGeomDrawCmdGenPipeline(const fs::path& csPath)
{
    if (!LoadShaderSpirVCode(csPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", csPath.string().c_str());
    }
    
    vkn::Shader shader;
    shader.Create(&s_vkDevice, VK_SHADER_STAGE_COMPUTE_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(shader, "GEOM_DRAW_CMD_GEN_COMPUTE_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_GEOM_DRAW_CMD_GEN];

    pso = s_computePSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .SetShader(shader)
        .SetLayout(s_PSOLayouts[PASS_ID_GEOM_DRAW_CMD_GEN])
        .Build();

    s_vkDevice.SetObjDebugName(pso, "GEOM_DRAW_CMD_GEN_PSO");
}


static void CreateZPassPipeline(const fs::path& vsPath, const fs::path& psPath)
{
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(vsShader, "ZPASS_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(psShader, "ZPASS_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_DEPTH];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[PASS_ID_DEPTH])
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
        .AddDynamicState(std::array{
            VK_DYNAMIC_STATE_VIEWPORT, 
            VK_DYNAMIC_STATE_SCISSOR,
        #ifndef ENG_BUILD_RELEASE
            VK_DYNAMIC_STATE_POLYGON_MODE_EXT
        #endif
        })        
        .SetDepthAttachment(s_depthRT.GetFormat());
    
    pso = s_graphicsPSOBuilder.Build();
    
    s_vkDevice.SetObjDebugName(pso, "ZPASS_PSO");
}


static void CreateHZBGenPipeline(const fs::path& csPath)
{
    if (!LoadShaderSpirVCode(csPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", csPath.string().c_str());
    }
    
    vkn::Shader shader;
    shader.Create(&s_vkDevice, VK_SHADER_STAGE_COMPUTE_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(shader, "HZB_GEN_COMPUTE_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_HZB_GEN];

    pso = s_computePSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .SetShader(shader)
        .SetLayout(s_PSOLayouts[PASS_ID_HZB_GEN])
        .Build();

    s_vkDevice.SetObjDebugName(pso, "HZB_GEN_PSO");
}


static void CreateCSMGeomCullingPipeline(const fs::path& csPath)
{
    if (!LoadShaderSpirVCode(csPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", csPath.string().c_str());
    }
    
    vkn::Shader shader;
    shader.Create(&s_vkDevice, VK_SHADER_STAGE_COMPUTE_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(shader, "CSM_GEOM_CULLING_COMPUTE_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_CSM_GEOM_CULLING];

    pso = s_computePSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .SetShader(shader)
        .SetLayout(s_PSOLayouts[PASS_ID_CSM_GEOM_CULLING])
        .Build();

    s_vkDevice.SetObjDebugName(pso, "CSM_GEOM_CULLING_PSO");
}


static void CreateCSMRenderPipeline(const fs::path& vsPath, const fs::path& psPath)
{
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(vsShader, "CSM_RENDER_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(psShader, "CSM_RENDER_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_CSM_RENDER];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[PASS_ID_CSM_RENDER])
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_FRONT_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(1.f)
    #ifdef ENG_REVERSED_Z
        .SetDepthTestState(VK_TRUE, VK_COMPARE_OP_GREATER_OR_EQUAL)
    #else
        .SetDepthTestState(VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
    #endif
        .SetDepthWriteState(VK_TRUE)
        .SetDepthBoundsTestState(VK_TRUE, 0.f, 1.f)
        .AddDynamicState(std::array{
            VK_DYNAMIC_STATE_VIEWPORT, 
            VK_DYNAMIC_STATE_SCISSOR,
        #ifndef ENG_BUILD_RELEASE
            VK_DYNAMIC_STATE_POLYGON_MODE_EXT
        #endif
        })        
        .SetDepthAttachment(s_csmRT.GetFormat());
    
    pso = s_graphicsPSOBuilder.Build();
    
    s_vkDevice.SetObjDebugName(pso, "CSM_RENDER_PSO");
}
    

static void CreateGBufferRenderPipeline(const fs::path& vsPath, const fs::path& psPath)
{
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(vsShader, "GBUFFER_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(psShader, "GBUFFER_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_GBUFFER];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[PASS_ID_GBUFFER])
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(1.f)
        .SetDepthTestState(VK_TRUE, VK_COMPARE_OP_EQUAL)
        .SetDepthWriteState(VK_FALSE)
        .SetDepthBoundsTestState(VK_TRUE, 0.f, 1.f)
        .AddDynamicState(std::array{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        #ifdef ENG_BUILD_DEBUG
            VK_DYNAMIC_STATE_DEPTH_COMPARE_OP, 
            VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
        #endif
        #ifndef ENG_BUILD_RELEASE
            VK_DYNAMIC_STATE_POLYGON_MODE_EXT,
        #endif
        });

    for (const vkn::Texture& colorRT : s_gbufferRTs) {
        s_graphicsPSOBuilder.AddColorAttachment(colorRT.GetFormat()); 
    }
    s_graphicsPSOBuilder.SetDepthAttachment(s_depthRT.GetFormat());
    
    pso = s_graphicsPSOBuilder.Build();
    
    s_vkDevice.SetObjDebugName(pso, "GBUFFER_PSO");
}


static void CreateDeferredLightingPipeline(const fs::path& vsPath, const fs::path& psPath)
{
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(vsShader, "DEFERRED_LIGHTING_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(psShader, "DEFERRED_LIGHTING_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_DEFERRED_LIGHTING];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[PASS_ID_DEFERRED_LIGHTING])
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(1.f)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .AddColorAttachment(s_colorRT16F.GetFormat());
    
    pso = s_graphicsPSOBuilder.Build();
    
    s_vkDevice.SetObjDebugName(pso, "DEFERRED_LIGHTING_PSO");
}


static void CreatePostProcessingPipeline(const fs::path& vsPath, const fs::path& psPath)
{
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(vsShader, "POST_PROCESSING_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(psShader, "POST_PROCESSING_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_POST_PROCESSING];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[PASS_ID_POST_PROCESSING])
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(1.f)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .AddColorAttachment(s_colorRT8U.GetFormat());
    
    pso = s_graphicsPSOBuilder.Build();
    
    s_vkDevice.SetObjDebugName(pso, "POST_PROCESSING_PSO");
}


static void CreateBackbufferPassPipeline(const fs::path& vsPath, const fs::path& psPath)
{
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(vsShader, "BACKBUFFER_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(psShader, "BACKBUFFER_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_BACKBUFFER];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[PASS_ID_BACKBUFFER])
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(1.f)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .AddColorAttachment(s_vkSwapchain.GetTextureFormat());
    
    pso = s_graphicsPSOBuilder.Build();
    
    s_vkDevice.SetObjDebugName(pso, "BACKBUFFER_PSO");
}


static void CreateSkyboxPipeline(const fs::path& vsPath, const fs::path& psPath)
{
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(vsShader, "SKYBOX_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(psShader, "SKYBOX_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_SKYBOX];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[PASS_ID_SKYBOX])
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
    
    s_vkDevice.SetObjDebugName(pso, "SKYBOX_PSO");
}


static void CreateIrradianceMapGenPipeline(const fs::path& csPath)
{
    if (!LoadShaderSpirVCode(csPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", csPath.string().c_str());
    }
    
    vkn::Shader shader;
    shader.Create(&s_vkDevice, VK_SHADER_STAGE_COMPUTE_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(shader, "IRRADIANCE_MAP_GEN_COMPUTE_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_IRRADIANCE_MAP_GEN];

    pso = s_computePSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .SetShader(shader)
        .SetLayout(s_PSOLayouts[PASS_ID_IRRADIANCE_MAP_GEN])
        .Build();

    s_vkDevice.SetObjDebugName(pso, "IRRADIANCE_MAP_GEN_PSO");
}


static void CreatePrefilteredEnvMapGenPipeline(const fs::path& csPath)
{
    if (!LoadShaderSpirVCode(csPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", csPath.string().c_str());
    }
    
    vkn::Shader shader;
    shader.Create(&s_vkDevice, VK_SHADER_STAGE_COMPUTE_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(shader, "PREFILT_ENV_MAP_GEN_COMPUTE_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_PREFILT_ENV_MAP_GEN];

    pso = s_computePSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .SetShader(shader)
        .SetLayout(s_PSOLayouts[PASS_ID_PREFILT_ENV_MAP_GEN])
        .Build();

    s_vkDevice.SetObjDebugName(pso, "PREFILT_ENV_MAP_GEN_PSO");
}


static void CreateBRDFIntegrationLUTGenPipeline(const fs::path& csPath)
{
    if (!LoadShaderSpirVCode(csPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", csPath.string().c_str());
    }
    
    vkn::Shader shader;
    shader.Create(&s_vkDevice, VK_SHADER_STAGE_COMPUTE_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(shader, "BRDF_LUT_GEN_COMPUTE_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_BRDF_LUT_GEN];

    pso = s_computePSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .SetShader(shader)
        .SetLayout(s_PSOLayouts[PASS_ID_BRDF_LUT_GEN])
        .Build();

    s_vkDevice.SetObjDebugName(pso, "BRDF_LUT_GEN_PSO");
}


static void CreateDbgDrawLinePipeline(const fs::path& vsPath, const fs::path& psPath)
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(vsShader, "DBG_DRAW_LINE_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(psShader, "DBG_DRAW_LINE_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_DBG_DRAW_LINES];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[PASS_ID_DBG_DRAW_LINES])
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
    
    s_vkDevice.SetObjDebugName(pso, "DBG_DRAW_LINES_PSO");
#endif
}


static void CreateDbgDrawTrianglePipeline(const fs::path& vsPath, const fs::path& psPath)
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(vsShader, "DBG_DRAW_TRIANGLE_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(psShader, "DBG_DRAW_TRIANGLE_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_DBG_DRAW_TRIANGLES];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[PASS_ID_DBG_DRAW_TRIANGLES])
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
    
    s_vkDevice.SetObjDebugName(pso, "DBG_DRAW_TRIANGLES_PSO");
#endif
}


static void CreateDbgRTViewPipeline(const fs::path& vsPath, const fs::path& psPath)
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    if (!LoadShaderSpirVCode(vsPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", vsPath.string().c_str());
    }
    
    vkn::Shader vsShader;
    vsShader.Create(&s_vkDevice, VK_SHADER_STAGE_VERTEX_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(vsShader, "DBG_RT_VIEW_VERTEX_SHADER");

    if (!LoadShaderSpirVCode(psPath, s_shaderCodeBuffer)) {
        VK_ASSERT_FAIL("Failed to load shader: %s", psPath.string().c_str());
    }
    
    vkn::Shader psShader;
    psShader.Create(&s_vkDevice, VK_SHADER_STAGE_FRAGMENT_BIT, s_shaderCodeBuffer);
    s_vkDevice.SetObjDebugName(psShader, "DBG_RT_VIEW_FRAGMENT_SHADER");

    vkn::PSO& pso = s_PSOs[PASS_ID_DBG_RT_VIEW];

    s_graphicsPSOBuilder.Reset()
        .SetFlags(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        .AddShader(vsShader)
        .AddShader(psShader)
        .SetLayout(s_PSOLayouts[PASS_ID_DBG_RT_VIEW])
        .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetRasterizerPolygonMode(VK_POLYGON_MODE_FILL)
        .SetRasterizerCullMode(VK_CULL_MODE_BACK_BIT)
        .SetRasterizerFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetRasterizerLineWidth(1.f)
        .AddDynamicState(std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .AddColorAttachment(s_colorRT8U.GetFormat());
    
    pso = s_graphicsPSOBuilder.Build();
    
    s_vkDevice.SetObjDebugName(pso, "DBG_RT_VIEW_PSO");
#endif
}


static void CreatePipelines()
{
    CreateGeomCullingPhase1PipelineLayout();
    CreateGeomCullingPhase2PipelineLayout();
    CreateGeomBatchingPipelineLayout();
    CreateGeomDrawCmdGenPipelineLayout();

    CreateZPassPipelineLayout();
    
    CreateHZBGenPipelineLayout();

    CreateCSMGeomCullingPipelineLayout();
    CreateCSMRenderPipelineLayout();
    
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
    CreateDbgRTViewPipelineLayout();


    CreateGeomCullingPhase1Pipeline(RND_SHADER_SPIRV_FULL_PATH("geom_culling_phase_1.cs.spv"));
    CreateGeomCullingPhase2Pipeline(RND_SHADER_SPIRV_FULL_PATH("geom_culling_phase_2.cs.spv"));
    CreateGeomBatchingPipeline(RND_SHADER_SPIRV_FULL_PATH("geom_batching.cs.spv"));
    CreateGeomDrawCmdGenPipeline(RND_SHADER_SPIRV_FULL_PATH("geom_draw_cmd_gen.cs.spv"));
    
    CreateZPassPipeline(RND_SHADER_SPIRV_FULL_PATH("zpass.vs.spv"), RND_SHADER_SPIRV_FULL_PATH("zpass.ps.spv"));
    
    CreateHZBGenPipeline(RND_SHADER_SPIRV_FULL_PATH("hzb.cs.spv"));

    CreateCSMGeomCullingPipeline(RND_SHADER_SPIRV_FULL_PATH("csm.cs.spv"));
    CreateCSMRenderPipeline(RND_SHADER_SPIRV_FULL_PATH("csm.vs.spv"), RND_SHADER_SPIRV_FULL_PATH("csm.ps.spv"));
    
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
    CreateDbgRTViewPipeline(RND_SHADER_SPIRV_FULL_PATH("dbg_rt_view.vs.spv"), RND_SHADER_SPIRV_FULL_PATH("dbg_rt_view.ps.spv"));
}


static void CreateCommonDbgTextures()
{
#ifndef ENG_BUILD_RELEASE
    vkn::AllocationInfo allocInfo = {};
    allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    std::array<vkn::TextureCreateInfo, COMMON_DBG_TEX_IDX_COUNT> texCreateInfos = {};

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

    texCreateInfos[COMMON_DBG_TEX_IDX_CHECKERBOARD].extent = { 128u, 128u, 1u };

    static constexpr std::array<const char*, COMMON_DBG_TEX_IDX_COUNT> texNames = {
        "COMMON_DBG_TEX_RED",
        "COMMON_DBG_TEX_GREEN",
        "COMMON_DBG_TEX_BLUE",
        "COMMON_DBG_TEX_BLACK",
        "COMMON_DBG_TEX_WHITE",
        "COMMON_DBG_TEX_GREY",
        "COMMON_DBG_TEX_CHECKERBOARD",
    };

    for (size_t i = 0; i < s_commonDbgTextures.size(); ++i) {
        s_commonDbgTextures[i].Create(texCreateInfos[i]);
        s_vkDevice.SetObjDebugName(s_commonDbgTextures[i], texNames[i]);
    }

    VkComponentMapping texMapping = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
            
    VkImageSubresourceRange texSubresourceRange = {};
    texSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    texSubresourceRange.baseMipLevel = 0;
    texSubresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    texSubresourceRange.baseArrayLayer = 0;
    texSubresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    for (size_t i = 0; i < s_commonDbgTextureViews.size(); ++i) {
        s_commonDbgTextureViews[i].Create(s_commonDbgTextures[i], texMapping, texSubresourceRange);
        s_vkDevice.SetObjDebugName(s_commonDbgTextureViews[i], texNames[i]);
    }
#endif
}


static void UploadGPUDbgTextures()
{
#ifndef ENG_BUILD_RELEASE
    auto UploadDbgTexture = [](vkn::CmdBuffer& cmdBuffer, size_t texIdx) -> void
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

        cmdBuffer.CmdCopyBuffer(s_commonStagingBuffer, texture, copyInfo);
    
        cmdBuffer
            .BeginBarrierList()
                .AddTextureBarrier(texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
            .Push();
    };

    uint8_t* pRedImageData = (uint8_t*)s_commonStagingBuffer.Map();
    pRedImageData[0] = 255;
    pRedImageData[1] = 0;
    pRedImageData[2] = 0;
    pRedImageData[3] = 255;
    s_commonStagingBuffer.Unmap();

    size_t writeTexIdx = 0;

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        UploadDbgTexture(cmdBuffer, writeTexIdx++);
    });

    uint8_t* pGreenImageData = (uint8_t*)s_commonStagingBuffer.Map();
    pGreenImageData[0] = 0;
    pGreenImageData[1] = 255;
    pGreenImageData[2] = 0;
    pGreenImageData[3] = 255;
    s_commonStagingBuffer.Unmap();

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        UploadDbgTexture(cmdBuffer, writeTexIdx++);
    });

    uint8_t* pBlueImageData = (uint8_t*)s_commonStagingBuffer.Map();
    pBlueImageData[0] = 0;
    pBlueImageData[1] = 0;
    pBlueImageData[2] = 255;
    pBlueImageData[3] = 255;
    s_commonStagingBuffer.Unmap();

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        UploadDbgTexture(cmdBuffer, writeTexIdx++);
    });

    uint8_t* pBlackImageData = (uint8_t*)s_commonStagingBuffer.Map();
    pBlackImageData[0] = 0;
    pBlackImageData[1] = 0;
    pBlackImageData[2] = 0;
    pBlackImageData[3] = 255;
    s_commonStagingBuffer.Unmap();

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        UploadDbgTexture(cmdBuffer, writeTexIdx++);
    });

    uint8_t* pWhiteImageData = (uint8_t*)s_commonStagingBuffer.Map();
    pWhiteImageData[0] = 255;
    pWhiteImageData[1] = 255;
    pWhiteImageData[2] = 255;
    pWhiteImageData[3] = 255;
    s_commonStagingBuffer.Unmap();

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        UploadDbgTexture(cmdBuffer, writeTexIdx++);
    });

    uint8_t* pGreyImageData = (uint8_t*)s_commonStagingBuffer.Map();
    pGreyImageData[0] = 128;
    pGreyImageData[1] = 128;
    pGreyImageData[2] = 128;
    pGreyImageData[3] = 255;
    s_commonStagingBuffer.Unmap();

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        UploadDbgTexture(cmdBuffer, writeTexIdx++);
    });


    vkn::Texture& checkerboardTex = s_commonDbgTextures[COMMON_DBG_TEX_IDX_CHECKERBOARD];

    uint32_t* pCheckerboardImageData = (uint32_t*)s_commonStagingBuffer.Map();

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
    s_commonStagingBuffer.Unmap();

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        UploadDbgTexture(cmdBuffer, writeTexIdx++);
    });
#endif
}


static void CreateGeomCullingAndInstancingResources()
{
    vkn::AllocationInfo allocInfo = {};
    allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    for (size_t phase = 0; phase < GEOM_CULLING_PHASES_COUNT; ++phase) {
        for (size_t queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            // TODO: we can caclulate actual instance count for certain queue during scene loading and allocate buffers with that sizes
            s_visGeomIDQueueBuffers[phase][queue].Create(
                &s_vkDevice, 
                s_cpuInstData.size() * sizeof(glm::uint), 
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT, 
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_visGeomIDQueueBuffers[phase][queue], "%s_VIS_INST_ID_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], phase);
            
            s_geomBatchQueueBuffers[phase][queue].Create(
                &s_vkDevice, 
                s_cpuInstData.size() * sizeof(GEOM_BATCH), 
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT, 
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_geomBatchQueueBuffers[phase][queue], "%s_BATCH_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], phase);

            s_sortedVisGeomIDQueueBuffers[phase][queue].Create(
                &s_vkDevice, 
                s_cpuInstData.size() * sizeof(glm::uint), 
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT, 
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_sortedVisGeomIDQueueBuffers[phase][queue], "%s_SORTED_VIS_INST_ID_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], phase);

            s_geomDrawCmdQueueBuffers[phase][queue].Create(
                &s_vkDevice,
                s_cpuInstData.size() * sizeof(COMMON_CMD_DRAW_INDEXED_INDIRECT),
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT,
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_geomDrawCmdQueueBuffers[phase][queue], "%s_DRAW_CMD_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], phase);
            
            s_geomBatchDispatchCmdBuffers[phase][queue].Create(
                &s_vkDevice,
                sizeof(COMMON_CMD_DISPATCH_INDIRECT),
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT,
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_geomBatchDispatchCmdBuffers[phase][queue], "%s_GEOM_BATCH_DISPATCH_CMD_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], phase);
            
            s_geomDrawCmdGenDispatchCmdBuffers[phase][queue].Create(
                &s_vkDevice,
                sizeof(COMMON_CMD_DISPATCH_INDIRECT),
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT,
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_geomDrawCmdGenDispatchCmdBuffers[phase][queue], "%s_GEOM_DRAW_CMD_GEN_DISPATCH_CMD_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], phase);
            
            s_visGeomIDQueueSizeBuffers[phase][queue].Create(
                &s_vkDevice,
                sizeof(glm::uint), 
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT,
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_visGeomIDQueueSizeBuffers[phase][queue], "%s_VIS_INST_ID_QUEUE_SIZE_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], phase);

            s_geomBatchQueueSizeBuffers[phase][queue].Create(
                &s_vkDevice,
                sizeof(glm::uint), 
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT,
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_geomBatchQueueSizeBuffers[phase][queue], "%s_BATCH_QUEUE_SIZE_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], phase);

            s_sortedVisGeomIDQueueSizeBuffers[phase][queue].Create(
                &s_vkDevice,
                sizeof(glm::uint), 
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT,
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_sortedVisGeomIDQueueSizeBuffers[phase][queue], "%s_SORTED_VIS_INST_ID_QUEUE_SIZE_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], phase);
        }
    }

    s_geomInstVisFlagsBuffer.Create(
        &s_vkDevice,
        (uint32_t)glm::ceil(s_cpuInstData.size() / 32.f) * sizeof(glm::uint), 
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
        allocInfo
    );
    s_vkDevice.SetObjDebugName(s_geomInstVisFlagsBuffer, "GEOM_INST_VIS_FLAGS_BUFFER");


    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        cmdBuffer
            .BeginBarrierList()
                .AddBufferBarrier(s_geomInstVisFlagsBuffer, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
            .Push();

        cmdBuffer.CmdFillBuffer(s_geomInstVisFlagsBuffer, 0);
    });
}


static void CreateCSMGeomCullingAndInstancingResources()
{
    vkn::AllocationInfo allocInfo = {};
    allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vkn::TextureCreateInfo rtCreateInfo = {};
    rtCreateInfo.pDevice = &s_vkDevice;
    rtCreateInfo.type = VK_IMAGE_TYPE_2D;
    rtCreateInfo.format = VK_FORMAT_D32_SFLOAT;
    rtCreateInfo.extent = VkExtent3D{ CSM_CASCADE_RT_SIZE, CSM_CASCADE_RT_SIZE, 1u };
    rtCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    rtCreateInfo.flags = 0;
    rtCreateInfo.mipLevels = 1;
    rtCreateInfo.arrayLayers = COMMON_CSM_CASCADE_COUNT;
    rtCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    rtCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    rtCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    rtCreateInfo.pAllocInfo = &allocInfo;

    s_csmRT.Create(rtCreateInfo);
    s_vkDevice.SetObjDebugName(s_csmRT, "CSM_DEPTH_RT");

    VkComponentMapping mapping = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

    for (size_t cascade = 0; cascade < COMMON_CSM_CASCADE_COUNT; ++cascade) {
        for (size_t queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            // TODO: we can caclulate actual instance count for certain queue during scene loading and allocate buffers with that sizes
            s_csmVisGeomIDQueueBuffers[cascade][queue].Create(
                &s_vkDevice, 
                s_cpuInstData.size() * sizeof(glm::uint), 
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT, 
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_csmVisGeomIDQueueBuffers[cascade][queue], "%s_CSM_VIS_INST_ID_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], cascade);
            
            s_csmGeomBatchQueueBuffers[cascade][queue].Create(
                &s_vkDevice, 
                s_cpuInstData.size() * sizeof(GEOM_BATCH), 
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT, 
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_csmGeomBatchQueueBuffers[cascade][queue], "%s_CSM_BATCH_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], cascade);

            s_csmSortedVisGeomIDQueueBuffers[cascade][queue].Create(
                &s_vkDevice, 
                s_cpuInstData.size() * sizeof(glm::uint), 
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT, 
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_csmSortedVisGeomIDQueueBuffers[cascade][queue], "%s_CSM_SORTED_VIS_INST_ID_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], cascade);

            s_csmGeomDrawCmdQueueBuffers[cascade][queue].Create(
                &s_vkDevice,
                s_cpuInstData.size() * sizeof(COMMON_CMD_DRAW_INDEXED_INDIRECT),
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT,
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_csmGeomDrawCmdQueueBuffers[cascade][queue], "%s_CSM_DRAW_CMD_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], cascade);
            
            s_csmGeomBatchDispatchCmdBuffers[cascade][queue].Create(
                &s_vkDevice,
                sizeof(COMMON_CMD_DISPATCH_INDIRECT),
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT,
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_csmGeomBatchDispatchCmdBuffers[cascade][queue], "%s_CSM_GEOM_BATCH_DISPATCH_CMD_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], cascade);
            
            s_csmGeomDrawCmdGenDispatchCmdBuffers[cascade][queue].Create(
                &s_vkDevice,
                sizeof(COMMON_CMD_DISPATCH_INDIRECT),
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT,
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_csmGeomDrawCmdGenDispatchCmdBuffers[cascade][queue], "%s_CSM_GEOM_DRAW_CMD_GEN_DISPATCH_CMD_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], cascade);

            s_csmVisGeomIDQueueSizeBuffers[cascade][queue].Create(
                &s_vkDevice,
                sizeof(glm::uint), 
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT,
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_csmVisGeomIDQueueSizeBuffers[cascade][queue], "%s_CSM_VIS_INST_ID_QUEUE_SIZE_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], cascade);

            s_csmGeomBatchQueueSizeBuffers[cascade][queue].Create(
                &s_vkDevice,
                sizeof(glm::uint), 
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT,
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_csmGeomBatchQueueSizeBuffers[cascade][queue], "%s_CSM_BATCH_QUEUE_SIZE_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], cascade);

            s_csmSortedVisGeomIDQueueSizeBuffers[cascade][queue].Create(
                &s_vkDevice,
                sizeof(glm::uint), 
                VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT,
                allocInfo
            );
            s_vkDevice.SetObjDebugName(s_csmSortedVisGeomIDQueueSizeBuffers[cascade][queue], "%s_CSM_SORTED_VIS_INST_ID_QUEUE_SIZE_BUFFER_%zu", GEOM_QUEUE_DBG_NAMES[queue], cascade);
        }

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        subresourceRange.baseArrayLayer = cascade;
        subresourceRange.layerCount = 1;

        s_csmRTViews[cascade].Create(s_csmRT, mapping, subresourceRange);
        s_vkDevice.SetObjDebugName(s_csmRTViews[cascade], "CSM_DEPTH_RT_VIEW_%zu", cascade);
    }

    vkn::TextureViewCreateInfo csmRTViewArrayCreateInfo = {};
    csmRTViewArrayCreateInfo.pOwner = &s_csmRT;
    csmRTViewArrayCreateInfo.type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    csmRTViewArrayCreateInfo.format = s_csmRT.GetFormat();
    csmRTViewArrayCreateInfo.components = mapping;
    csmRTViewArrayCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    csmRTViewArrayCreateInfo.subresourceRange.baseMipLevel = 0;
    csmRTViewArrayCreateInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    csmRTViewArrayCreateInfo.subresourceRange.baseArrayLayer = 0;
    csmRTViewArrayCreateInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    s_csmRTViewArray.Create(csmRTViewArrayCreateInfo);
    s_vkDevice.SetObjDebugName(s_csmRTViewArray, "CSM_DEPTH_RT_VIEW_ARRAY");
}


static void CreateCommonSamplers()
{
    s_commonSamplers.resize(SAMPLER_IDX_COUNT);

    std::vector<vkn::SamplerCreateInfo> samplerCreateInfo(SAMPLER_IDX_COUNT);

    samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT].pDevice = &s_vkDevice;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT].magFilter = VK_FILTER_NEAREST;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT].minFilter = VK_FILTER_NEAREST;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT].mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT].addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT].addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT].addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT].mipLodBias = 0.f;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT].anisotropyEnable = VK_FALSE;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT].compareEnable = VK_FALSE;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT].minLod = 0.f;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT].maxLod = VK_LOD_CLAMP_NONE;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT].unnormalizedCoordinates = VK_FALSE;

    samplerCreateInfo[SAMPLER_IDX_NEAREST_MIRRORED_REPEAT] = samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT];
    samplerCreateInfo[SAMPLER_IDX_NEAREST_MIRRORED_REPEAT].addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_MIRRORED_REPEAT].addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_MIRRORED_REPEAT].addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

    samplerCreateInfo[SAMPLER_IDX_NEAREST_CLAMP_TO_EDGE] = samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT];
    samplerCreateInfo[SAMPLER_IDX_NEAREST_CLAMP_TO_EDGE].addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_CLAMP_TO_EDGE].addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_CLAMP_TO_EDGE].addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    samplerCreateInfo[SAMPLER_IDX_NEAREST_CLAMP_TO_BORDER] = samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT];
    samplerCreateInfo[SAMPLER_IDX_NEAREST_CLAMP_TO_BORDER].addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_CLAMP_TO_BORDER].addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_CLAMP_TO_BORDER].addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_CLAMP_TO_BORDER].borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    samplerCreateInfo[SAMPLER_IDX_NEAREST_MIRROR_CLAMP_TO_EDGE] = samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT];
    samplerCreateInfo[SAMPLER_IDX_NEAREST_MIRROR_CLAMP_TO_EDGE].addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_MIRROR_CLAMP_TO_EDGE].addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    samplerCreateInfo[SAMPLER_IDX_NEAREST_MIRROR_CLAMP_TO_EDGE].addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;

    samplerCreateInfo[SAMPLER_IDX_LINEAR_REPEAT] = samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT];
    samplerCreateInfo[SAMPLER_IDX_LINEAR_REPEAT].magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo[SAMPLER_IDX_LINEAR_REPEAT].minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo[SAMPLER_IDX_LINEAR_REPEAT].mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    samplerCreateInfo[SAMPLER_IDX_LINEAR_MIRRORED_REPEAT] = samplerCreateInfo[SAMPLER_IDX_LINEAR_REPEAT];
    samplerCreateInfo[SAMPLER_IDX_LINEAR_MIRRORED_REPEAT].addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerCreateInfo[SAMPLER_IDX_LINEAR_MIRRORED_REPEAT].addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerCreateInfo[SAMPLER_IDX_LINEAR_MIRRORED_REPEAT].addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

    samplerCreateInfo[SAMPLER_IDX_LINEAR_CLAMP_TO_EDGE] = samplerCreateInfo[SAMPLER_IDX_LINEAR_REPEAT];
    samplerCreateInfo[SAMPLER_IDX_LINEAR_CLAMP_TO_EDGE].addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo[SAMPLER_IDX_LINEAR_CLAMP_TO_EDGE].addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo[SAMPLER_IDX_LINEAR_CLAMP_TO_EDGE].addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    
    samplerCreateInfo[SAMPLER_IDX_LINEAR_CLAMP_TO_BORDER] = samplerCreateInfo[SAMPLER_IDX_LINEAR_REPEAT];
    samplerCreateInfo[SAMPLER_IDX_LINEAR_CLAMP_TO_BORDER].addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo[SAMPLER_IDX_LINEAR_CLAMP_TO_BORDER].addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo[SAMPLER_IDX_LINEAR_CLAMP_TO_BORDER].addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCreateInfo[SAMPLER_IDX_LINEAR_CLAMP_TO_BORDER].borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    
    samplerCreateInfo[SAMPLER_IDX_LINEAR_MIRROR_CLAMP_TO_EDGE] = samplerCreateInfo[SAMPLER_IDX_LINEAR_REPEAT];
    samplerCreateInfo[SAMPLER_IDX_LINEAR_MIRROR_CLAMP_TO_EDGE].addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    samplerCreateInfo[SAMPLER_IDX_LINEAR_MIRROR_CLAMP_TO_EDGE].addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    samplerCreateInfo[SAMPLER_IDX_LINEAR_MIRROR_CLAMP_TO_EDGE].addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;


    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_NEAREST_REPEAT] = samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT];
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_NEAREST_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_NEAREST_REPEAT].maxAnisotropy = 2.f;
    
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_NEAREST_MIRRORED_REPEAT] = samplerCreateInfo[SAMPLER_IDX_NEAREST_MIRRORED_REPEAT];
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_NEAREST_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_NEAREST_MIRRORED_REPEAT].maxAnisotropy = 2.f;
    
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_NEAREST_CLAMP_TO_EDGE] = samplerCreateInfo[SAMPLER_IDX_NEAREST_CLAMP_TO_EDGE];
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_NEAREST_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_NEAREST_CLAMP_TO_EDGE].maxAnisotropy = 2.f;
    
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_NEAREST_CLAMP_TO_BORDER] = samplerCreateInfo[SAMPLER_IDX_NEAREST_CLAMP_TO_BORDER];
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_NEAREST_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_NEAREST_CLAMP_TO_BORDER].maxAnisotropy = 2.f;
    
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_NEAREST_MIRROR_CLAMP_TO_EDGE] = samplerCreateInfo[SAMPLER_IDX_NEAREST_MIRROR_CLAMP_TO_EDGE];
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_NEAREST_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_NEAREST_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 2.f;
    
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_LINEAR_REPEAT] = samplerCreateInfo[SAMPLER_IDX_LINEAR_REPEAT];
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_LINEAR_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_LINEAR_REPEAT].maxAnisotropy = 2.f;

    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_LINEAR_MIRRORED_REPEAT] = samplerCreateInfo[SAMPLER_IDX_LINEAR_MIRRORED_REPEAT];
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_LINEAR_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_LINEAR_MIRRORED_REPEAT].maxAnisotropy = 2.f;

    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_LINEAR_CLAMP_TO_EDGE] = samplerCreateInfo[SAMPLER_IDX_LINEAR_CLAMP_TO_EDGE];
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_LINEAR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_LINEAR_CLAMP_TO_EDGE].maxAnisotropy = 2.f;

    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_LINEAR_CLAMP_TO_BORDER] = samplerCreateInfo[SAMPLER_IDX_LINEAR_CLAMP_TO_BORDER];
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_LINEAR_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_LINEAR_CLAMP_TO_BORDER].maxAnisotropy = 2.f;

    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_LINEAR_MIRROR_CLAMP_TO_EDGE] = samplerCreateInfo[SAMPLER_IDX_LINEAR_MIRROR_CLAMP_TO_EDGE];
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_LINEAR_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_2X_LINEAR_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 2.f;


    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_NEAREST_REPEAT] = samplerCreateInfo[SAMPLER_IDX_NEAREST_REPEAT];
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_NEAREST_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_NEAREST_REPEAT].maxAnisotropy = 4.f;

    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_NEAREST_MIRRORED_REPEAT] = samplerCreateInfo[SAMPLER_IDX_NEAREST_MIRRORED_REPEAT];
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_NEAREST_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_NEAREST_MIRRORED_REPEAT].maxAnisotropy = 4.f;
    
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_NEAREST_CLAMP_TO_EDGE] = samplerCreateInfo[SAMPLER_IDX_NEAREST_CLAMP_TO_EDGE];
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_NEAREST_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_NEAREST_CLAMP_TO_EDGE].maxAnisotropy = 4.f;
    
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_NEAREST_CLAMP_TO_BORDER] = samplerCreateInfo[SAMPLER_IDX_NEAREST_CLAMP_TO_BORDER];
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_NEAREST_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_NEAREST_CLAMP_TO_BORDER].maxAnisotropy = 4.f;
    
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_NEAREST_MIRROR_CLAMP_TO_EDGE] = samplerCreateInfo[SAMPLER_IDX_NEAREST_MIRROR_CLAMP_TO_EDGE];
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_NEAREST_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_NEAREST_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 4.f;

    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_LINEAR_REPEAT] = samplerCreateInfo[SAMPLER_IDX_LINEAR_REPEAT];
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_LINEAR_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_LINEAR_REPEAT].maxAnisotropy = 4.f;
    
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_LINEAR_MIRRORED_REPEAT] = samplerCreateInfo[SAMPLER_IDX_LINEAR_MIRRORED_REPEAT];
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_LINEAR_MIRRORED_REPEAT].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_LINEAR_MIRRORED_REPEAT].maxAnisotropy = 4.f;
    
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_LINEAR_CLAMP_TO_EDGE] = samplerCreateInfo[SAMPLER_IDX_LINEAR_CLAMP_TO_EDGE];
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_LINEAR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_LINEAR_CLAMP_TO_EDGE].maxAnisotropy = 4.f;
    
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_LINEAR_CLAMP_TO_BORDER] = samplerCreateInfo[SAMPLER_IDX_LINEAR_CLAMP_TO_BORDER];
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_LINEAR_CLAMP_TO_BORDER].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_LINEAR_CLAMP_TO_BORDER].maxAnisotropy = 4.f;
    
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_LINEAR_MIRROR_CLAMP_TO_EDGE] = samplerCreateInfo[SAMPLER_IDX_LINEAR_MIRROR_CLAMP_TO_EDGE];
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_LINEAR_MIRROR_CLAMP_TO_EDGE].anisotropyEnable = VK_TRUE;
    samplerCreateInfo[SAMPLER_IDX_ANISO_4X_LINEAR_MIRROR_CLAMP_TO_EDGE].maxAnisotropy = 4.f;

    for (size_t i = 0; i < samplerCreateInfo.size(); ++i) {
        s_commonSamplers[i].Create(samplerCreateInfo[i]);
        s_vkDevice.SetObjDebugName(s_commonSamplers[i], COMMON_SAMPLERS_DBG_NAMES[i]);
    }
}


static void WriteGeomCullingDescriptorSet(uint32_t phase, GEOM_QUEUE queue)
{
    CORE_ASSERT(phase < GEOM_CULLING_PHASES_COUNT);
    CORE_ASSERT(queue < GEOM_QUEUE_COUNT);
    
    const DescSetID setID = phase == 0 ? DESC_SET_ID_GEOM_CULLING_PHASE_1 : DESC_SET_ID_GEOM_CULLING_PHASE_2;

    s_descriptorBuffer.WriteDescriptor(setID, GEOM_CULL_VIS_INST_ID_QUEUES_UAV_DESCRIPTOR_SLOT, 
        queue, s_visGeomIDQueueBuffers[phase][queue]);

    s_descriptorBuffer.WriteDescriptor(setID, GEOM_CULL_VIS_INST_ID_QUEUE_SIZES_UAV_DESCRIPTOR_SLOT, 
        queue, s_visGeomIDQueueSizeBuffers[phase][queue]);
        
    s_descriptorBuffer.WriteDescriptor(setID, GEOM_CULL_VIS_FLAGS_UAV_DESCRIPTOR_SLOT, 
        0, s_geomInstVisFlagsBuffer);

    s_descriptorBuffer.WriteDescriptor(setID, GEOM_CULL_BATCH_DISPATCH_CMDS_UAV_DESCRIPTOR_SLOT, 
        queue, s_geomBatchDispatchCmdBuffers[phase][queue]);
}


static void WriteGeomCullingDescriptorSet()
{
    for (uint32_t phase = 0; phase < GEOM_CULLING_PHASES_COUNT; ++phase) {
        for (uint32_t queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            WriteGeomCullingDescriptorSet(phase, (GEOM_QUEUE)queue);
        }
    }
}


static void WriteGeomBatchingDescriptorSet(uint32_t phase, GEOM_QUEUE queue)
{
    CORE_ASSERT(phase < GEOM_CULLING_PHASES_COUNT);
    CORE_ASSERT(queue < GEOM_QUEUE_COUNT);
    
    DescSetID setID;

    switch(queue) {
        case GEOM_QUEUE_OPAQUE:
            setID = phase == 0 ? DESC_SET_ID_GEOM_BATCHING_OPAQUE_PHASE_1 : DESC_SET_ID_GEOM_BATCHING_OPAQUE_PHASE_2;
            break;
        case GEOM_QUEUE_AKILL:
            setID = phase == 0 ? DESC_SET_ID_GEOM_BATCHING_AKILL_PHASE_1 : DESC_SET_ID_GEOM_BATCHING_AKILL_PHASE_2;
            break;
    }

    s_descriptorBuffer.WriteDescriptor(setID, GEOM_BATCH_VIS_INST_ID_QUEUE_DESCRIPTOR_SLOT, 0, s_visGeomIDQueueBuffers[phase][queue]);
    s_descriptorBuffer.WriteDescriptor(setID, GEOM_BATCH_VIS_INST_ID_QUEUE_SIZE_DESCRIPTOR_SLOT, 0, s_visGeomIDQueueSizeBuffers[phase][queue]);
    
    s_descriptorBuffer.WriteDescriptor(setID, GEOM_BATCH_BATCH_QUEUE_UAV_DESCRIPTOR_SLOT, 0, s_geomBatchQueueBuffers[phase][queue]);
    s_descriptorBuffer.WriteDescriptor(setID, GEOM_BATCH_BATCH_QUEUE_SIZE_UAV_DESCRIPTOR_SLOT, 0, s_geomBatchQueueSizeBuffers[phase][queue]);

    s_descriptorBuffer.WriteDescriptor(setID, GEOM_BATCH_SORTED_VIS_INST_ID_QUEUE_UAV_DESCRIPTOR_SLOT, 0, s_sortedVisGeomIDQueueBuffers[phase][queue]);
    s_descriptorBuffer.WriteDescriptor(setID, GEOM_BATCH_SORTED_VIS_INST_ID_QUEUE_SIZE_UAV_DESCRIPTOR_SLOT, 0, s_sortedVisGeomIDQueueSizeBuffers[phase][queue]);
    
    s_descriptorBuffer.WriteDescriptor(setID, GEOM_BATCH_DRAW_CMD_GEN_DISPATCH_CMD_UAV_DESCRIPTOR_SLOT, 0, s_geomDrawCmdGenDispatchCmdBuffers[phase][queue]);
}


static void WriteGeomBatchingDescriptorSet()
{
    for (uint32_t phase = 0; phase < GEOM_CULLING_PHASES_COUNT; ++phase) {
        for (uint32_t queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            WriteGeomBatchingDescriptorSet(phase, (GEOM_QUEUE)queue);
        }
    }
}


static void WriteGeomDrawCmdGenDescriptorSet(uint32_t phase, GEOM_QUEUE queue)
{
    CORE_ASSERT(phase < GEOM_CULLING_PHASES_COUNT);
    CORE_ASSERT(queue < GEOM_QUEUE_COUNT);
    
    DescSetID setID;

    switch(queue) {
        case GEOM_QUEUE_OPAQUE:
            setID = phase == 0 ? DESC_SET_ID_GEOM_DRAW_CMD_GEN_OPAQUE_PHASE_1 : DESC_SET_ID_GEOM_DRAW_CMD_GEN_OPAQUE_PHASE_2;
            break;
        case GEOM_QUEUE_AKILL:
            setID = phase == 0 ? DESC_SET_ID_GEOM_DRAW_CMD_GEN_AKILL_PHASE_1 : DESC_SET_ID_GEOM_DRAW_CMD_GEN_AKILL_PHASE_2;
            break;
    }

    s_descriptorBuffer.WriteDescriptor(setID, GEOM_DRAW_CMD_GEN_BATCH_QUEUE_DESCRIPTOR_SLOT, 0, s_geomBatchQueueBuffers[phase][queue]);
    s_descriptorBuffer.WriteDescriptor(setID, GEOM_DRAW_CMD_GEN_BATCH_QUEUE_SIZE_DESCRIPTOR_SLOT, 0, s_geomBatchQueueSizeBuffers[phase][queue]);

    s_descriptorBuffer.WriteDescriptor(setID, GEOM_DRAW_CMD_GEN_CMD_QUEUE_UAV_DESCRIPTOR_SLOT, 0, s_geomDrawCmdQueueBuffers[phase][queue]);
}


static void WriteGeomDrawCmdGenDescriptorSet()
{
    for (uint32_t phase = 0; phase < GEOM_CULLING_PHASES_COUNT; ++phase) {
        for (uint32_t queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            WriteGeomDrawCmdGenDescriptorSet(phase, (GEOM_QUEUE)queue);
        }
    }
}


static void WriteZPassDescriptorSet(uint32_t phase, GEOM_QUEUE queue)
{
    CORE_ASSERT(phase < GEOM_CULLING_PHASES_COUNT);
    CORE_ASSERT(queue < GEOM_QUEUE_COUNT);
    
    DescSetID setID;

    switch(queue) {
        case GEOM_QUEUE_OPAQUE:
            setID = phase == 0 ? DESC_SET_ID_DEPTH_OPAQUE_PHASE_1 : DESC_SET_ID_DEPTH_OPAQUE_PHASE_2;
            break;
        case GEOM_QUEUE_AKILL:
            setID = phase == 0 ? DESC_SET_ID_DEPTH_AKILL_PHASE_1 : DESC_SET_ID_DEPTH_AKILL_PHASE_2;
            break;
    }

    s_descriptorBuffer.WriteDescriptor(setID, ZPASS_INST_ID_QUEUE_DESCRIPTOR_SLOT, 0, s_sortedVisGeomIDQueueBuffers[phase][queue]);
}


static void WriteZPassDescriptorSet()
{
    for (uint32_t phase = 0; phase < GEOM_CULLING_PHASES_COUNT; ++phase) {
        for (uint32_t queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            WriteZPassDescriptorSet(phase, (GEOM_QUEUE)queue);
        }
    }
}


static void WriteHZBGenDescriptorSets()
{
    for (uint32_t i = 0; i < s_HZB.GetMipCount(); ++i) {
        vkn::TextureView& mip = s_HZBMipViews[i];

        s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_HZB_GEN, HZB_SRC_MIPS_DESCRIPTOR_SLOT, i, mip, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_HZB_GEN, HZB_DST_MIPS_UAV_DESCRIPTOR_SLOT, i, mip, VK_IMAGE_LAYOUT_GENERAL);
    }

    // First source mip must contain original depth buffer
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_HZB_GEN, HZB_SRC_MIPS_DESCRIPTOR_SLOT, 0, s_depthRTView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}


static void WriteCSMGeomCullingDescriptorSet(uint32_t cascade, GEOM_QUEUE queue)
{
    CORE_ASSERT(cascade < COMMON_CSM_CASCADE_COUNT);
    CORE_ASSERT(queue < GEOM_QUEUE_COUNT);

    static constexpr DescSetID descID = DESC_SET_ID_CSM_GEOM_CULLING;
    const uint32_t index = CSMGetBufferIndex(cascade, queue);

    s_descriptorBuffer.WriteDescriptor(descID, CSM_VIS_INST_ID_QUEUES_UAV_DESCRIPTOR_SLOT, 
        index, s_csmVisGeomIDQueueBuffers[cascade][queue]);

    s_descriptorBuffer.WriteDescriptor(descID, CSM_VIS_INST_ID_QUEUE_SIZES_UAV_DESCRIPTOR_SLOT, 
        index, s_csmVisGeomIDQueueSizeBuffers[cascade][queue]);

    s_descriptorBuffer.WriteDescriptor(descID, CSM_BATCH_DISPATCH_CMDS_UAV_DESCRIPTOR_SLOT, 
        index, s_csmGeomBatchDispatchCmdBuffers[cascade][queue]);
}


static void WriteCSMGeomCullingDescriptorSet()
{
    for (uint32_t cascade = 0; cascade < COMMON_CSM_CASCADE_COUNT; ++cascade) {
        for (uint32_t queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            WriteCSMGeomCullingDescriptorSet(cascade, (GEOM_QUEUE)queue);
        }
    }
}


static void WriteCSMGeomBatchingDescriptorSet(uint32_t cascade, GEOM_QUEUE queue)
{
    CORE_ASSERT(cascade < COMMON_CSM_CASCADE_COUNT);
    CORE_ASSERT(queue < GEOM_QUEUE_COUNT);
    
    DescSetID setID;

    switch(queue) {
        case GEOM_QUEUE_OPAQUE:
            setID = static_cast<DescSetID>(DESC_SET_ID_CSM_GEOM_BATCHING_OPAQUE_CASCADE_0 + cascade);
            break;
        case GEOM_QUEUE_AKILL:
            setID = static_cast<DescSetID>(DESC_SET_ID_CSM_GEOM_BATCHING_AKILL_CASCADE_0 + cascade);
            break;
    }

    s_descriptorBuffer.WriteDescriptor(setID, GEOM_BATCH_VIS_INST_ID_QUEUE_DESCRIPTOR_SLOT, 0, s_csmVisGeomIDQueueBuffers[cascade][queue]);
    s_descriptorBuffer.WriteDescriptor(setID, GEOM_BATCH_VIS_INST_ID_QUEUE_SIZE_DESCRIPTOR_SLOT, 0, s_csmVisGeomIDQueueSizeBuffers[cascade][queue]);
    
    s_descriptorBuffer.WriteDescriptor(setID, GEOM_BATCH_BATCH_QUEUE_UAV_DESCRIPTOR_SLOT, 0, s_csmGeomBatchQueueBuffers[cascade][queue]);
    s_descriptorBuffer.WriteDescriptor(setID, GEOM_BATCH_BATCH_QUEUE_SIZE_UAV_DESCRIPTOR_SLOT, 0, s_csmGeomBatchQueueSizeBuffers[cascade][queue]);

    s_descriptorBuffer.WriteDescriptor(setID, GEOM_BATCH_SORTED_VIS_INST_ID_QUEUE_UAV_DESCRIPTOR_SLOT, 0, s_csmSortedVisGeomIDQueueBuffers[cascade][queue]);
    s_descriptorBuffer.WriteDescriptor(setID, GEOM_BATCH_SORTED_VIS_INST_ID_QUEUE_SIZE_UAV_DESCRIPTOR_SLOT, 0, s_csmSortedVisGeomIDQueueSizeBuffers[cascade][queue]);
    
    s_descriptorBuffer.WriteDescriptor(setID, GEOM_BATCH_DRAW_CMD_GEN_DISPATCH_CMD_UAV_DESCRIPTOR_SLOT, 0, s_csmGeomDrawCmdGenDispatchCmdBuffers[cascade][queue]);
}


static void WriteCSMGeomBatchingDescriptorSet()
{
    for (uint32_t cascade = 0; cascade < COMMON_CSM_CASCADE_COUNT; ++cascade) {
        for (uint32_t queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            WriteCSMGeomBatchingDescriptorSet(cascade, (GEOM_QUEUE)queue);
        }
    }
}


static void WriteCSMGeomDrawCmdGenDescriptorSet(uint32_t cascade, GEOM_QUEUE queue)
{
    CORE_ASSERT(cascade < COMMON_CSM_CASCADE_COUNT);
    CORE_ASSERT(queue < GEOM_QUEUE_COUNT);
    
    DescSetID setID;

    switch(queue) {
        case GEOM_QUEUE_OPAQUE:
            setID = static_cast<DescSetID>(DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_OPAQUE_CASCADE_0 + cascade);
            break;
        case GEOM_QUEUE_AKILL:
            setID = static_cast<DescSetID>(DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_AKILL_CASCADE_0 + cascade);
            break;
    }

    s_descriptorBuffer.WriteDescriptor(setID, GEOM_DRAW_CMD_GEN_BATCH_QUEUE_DESCRIPTOR_SLOT, 0, s_csmGeomBatchQueueBuffers[cascade][queue]);
    s_descriptorBuffer.WriteDescriptor(setID, GEOM_DRAW_CMD_GEN_BATCH_QUEUE_SIZE_DESCRIPTOR_SLOT, 0, s_csmGeomBatchQueueSizeBuffers[cascade][queue]);

    s_descriptorBuffer.WriteDescriptor(setID, GEOM_DRAW_CMD_GEN_CMD_QUEUE_UAV_DESCRIPTOR_SLOT, 0, s_csmGeomDrawCmdQueueBuffers[cascade][queue]);
}


static void WriteCSMGeomDrawCmdGenDescriptorSet()
{
    for (uint32_t cascade = 0; cascade < COMMON_CSM_CASCADE_COUNT; ++cascade) {
        for (uint32_t queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            WriteCSMGeomDrawCmdGenDescriptorSet(cascade, (GEOM_QUEUE)queue);
        }
    }
}


static void WriteCSMRenderDescriptorSet(uint32_t cascade, GEOM_QUEUE queue)
{
    CORE_ASSERT(cascade < COMMON_CSM_CASCADE_COUNT);
    CORE_ASSERT(queue < GEOM_QUEUE_COUNT);

    static constexpr DescSetID setID = DESC_SET_ID_CSM_RENDER;
    const uint32_t index = CSMGetBufferIndex(cascade, queue);

    s_descriptorBuffer.WriteDescriptor(setID, CSM_INST_ID_QUEUE_DESCRIPTOR_SLOT, index, s_csmSortedVisGeomIDQueueBuffers[cascade][queue]);
}


static void WriteCSMRenderDescriptorSet()
{
    for (uint32_t cascade = 0; cascade < COMMON_CSM_CASCADE_COUNT; ++cascade) {
        for (uint32_t queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            WriteCSMRenderDescriptorSet(cascade, (GEOM_QUEUE)queue);
        }
    }
}


static void WriteGBufferDescriptorSet(uint32_t phase, GEOM_QUEUE queue)
{
    CORE_ASSERT(phase < GEOM_CULLING_PHASES_COUNT);
    CORE_ASSERT(queue < GEOM_QUEUE_COUNT);
    
    DescSetID setID;

    switch(queue) {
        case GEOM_QUEUE_OPAQUE:
            setID = phase == 0 ? DESC_SET_ID_GBUFFER_OPAQUE_PHASE_1 : DESC_SET_ID_GBUFFER_OPAQUE_PHASE_2;
            break;
        case GEOM_QUEUE_AKILL:
            setID = phase == 0 ? DESC_SET_ID_GBUFFER_AKILL_PHASE_1 : DESC_SET_ID_GBUFFER_AKILL_PHASE_2;
            break;
    }   

    s_descriptorBuffer.WriteDescriptor(setID, GBUFFER_INST_ID_QUEUE_DESCRIPTOR_SLOT, 0, s_sortedVisGeomIDQueueBuffers[phase][queue]);
}


static void WriteGBufferDescriptorSet()
{
    for (uint32_t phase = 0; phase < GEOM_CULLING_PHASES_COUNT; ++phase) {
        for (uint32_t queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            WriteGBufferDescriptorSet(phase, (GEOM_QUEUE)queue);
        }
    }
}


static void WriteDeferredLightingDescriptorSet()
{
    std::array<vkn::TextureView*, GBUFFER_RT_COUNT> gbufferViews = {};
    for (size_t i = 0; i < GBUFFER_RT_COUNT; ++i) {
        gbufferViews[i] = &s_gbufferRTViews[i];
    }

    for (size_t i = 0; i < GBUFFER_RT_COUNT; ++i) {
        s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DEFERRED_LIGHTING, DEFERRED_LIGHTING_GBUFFER_0_DESCRIPTOR_SLOT + i, 0, *gbufferViews[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DEFERRED_LIGHTING, DEFERRED_LIGHTING_DEPTH_DESCRIPTOR_SLOT, 0, s_depthRTView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DEFERRED_LIGHTING, DEFERRED_LIGHTING_IRRADIANCE_MAP_DESCRIPTOR_SLOT, 0, s_irradianceMapTextureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DEFERRED_LIGHTING, DEFERRED_LIGHTING_PREFILTERED_ENV_MAP_DESCRIPTOR_SLOT, 0, s_prefilteredEnvMapTextureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DEFERRED_LIGHTING, DEFERRED_LIGHTING_BRDF_LUT_DESCRIPTOR_SLOT, 0, s_brdfLUTTextureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DEFERRED_LIGHTING, DEFERRED_LIGHTING_CSM_DESCRIPTOR_SLOT, 0, s_csmRTViewArray, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}


static void WritePostProcessingDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_POST_PROCESSING, POST_PROCESSING_INPUT_COLOR_DESCRIPTOR_SLOT, 0, s_colorRTView16F, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}


static void WriteBackbufferPassDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_BACKBUFFER, BACKBUFFER_INPUT_COLOR_DESCRIPTOR_SLOT, 0, s_colorRTView8U, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}


static void WriteSkyboxDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_SKYBOX, SKYBOX_TEXTURE_DESCRIPTOR_SLOT, 0, s_skyboxTextureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}


static void WriteIrradianceMapGenDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_IRRADIANCE_MAP_GEN, IRRADIANCE_MAP_GEN_ENV_MAP_DESCRIPTOR_SLOT, 0, s_skyboxTextureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_IRRADIANCE_MAP_GEN, IRRADIANCE_MAP_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT, 0, s_irradianceMapTextureViewRW, VK_IMAGE_LAYOUT_GENERAL);
}


static void WritePrefilteredEnvMapGenDescriptorSets()
{
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_PREFILT_ENV_MAP_GEN, PREFILTERED_ENV_MAP_GEN_ENV_MAP_DESCRIPTOR_SLOT, 0, s_skyboxTextureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    for (uint32_t i = 0; i < s_prefilteredEnvMapTextureViewRWs.size(); ++i) {
        s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_PREFILT_ENV_MAP_GEN, PREFILTERED_ENV_MAP_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT, i, s_prefilteredEnvMapTextureViewRWs[i], VK_IMAGE_LAYOUT_GENERAL);
    }
}


static void WriteBRDFIntegrationLUTGenDescriptorSet()
{
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_BRDF_LUT_GEN, BRDF_INTEGRATION_GEN_OUTPUT_UAV_DESCRIPTOR_SLOT, 0, s_brdfLUTTextureViewRW, VK_IMAGE_LAYOUT_GENERAL);
}


static void WriteCommonDescriptorSet()
{
    for (size_t i = 0; i < s_commonSamplers.size(); ++i) {
        s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_COMMON, COMMON_SAMPLERS_DESCRIPTOR_SLOT, i, s_commonSamplers[i]);
    }

#ifndef ENG_BUILD_RELEASE
    for (size_t i = 0; i < s_commonDbgTextureViews.size(); ++i) {
        s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_COMMON, COMMON_DBG_TEXTURES_DESCRIPTOR_SLOT, i, s_commonDbgTextureViews[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
#endif

    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_COMMON, COMMON_CB_DESCRIPTOR_SLOT, 0, s_commonConstBuffer);

#ifndef ENG_BUILD_RELEASE
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_COMMON, COMMON_DBG_CB_DESCRIPTOR_SLOT, 0, s_commonDbgConstBuffer);
#endif
    
    for (size_t i = 0; i < COMMON_GEOM_STREAM_COUNT; ++i) {
        s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_COMMON, COMMON_GEOM_STREAMS_DESCRIPTOR_SLOT, i, s_geomStreamBuffers[i]);
    }
    
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_COMMON, COMMON_MESH_LOD_BUFFER_DESCRIPTOR_SLOT, 0, s_commonMeshLODBuffer);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_COMMON, COMMON_MESH_BUFFER_DESCRIPTOR_SLOT, 0, s_commonMeshBuffer);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_COMMON, COMMON_MATERIALS_DESCRIPTOR_SLOT, 0, s_commonMaterialBuffer);

    for (size_t i = 0; i < s_commonMaterialTextureViews.size(); ++i) {
        s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_COMMON, COMMON_MTL_TEXTURES_DESCRIPTOR_SLOT, i, s_commonMaterialTextureViews[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_COMMON, COMMON_INST_BUFFER_DESCRIPTOR_SLOT, 0, s_commonInstBuffer);

    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_COMMON, COMMON_DEPTH_DESCRIPTOR_SLOT, 0, s_depthRTView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_COMMON, COMMON_HZB_DESCRIPTOR_SLOT, 0, s_HZBView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}


static void WriteDbgDrawLineDescriptorSet()
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DBG_DRAW_LINES, DBG_DRAW_LINES_VERTEX_BUFFER_DESCRIPTOR_SLOT, 0, s_dbgLineVertexDataGPU);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DBG_DRAW_LINES, DBG_DRAW_LINES_DATA_DESCRIPTOR_SLOT, 0, s_dbgLineDataGPU);
#endif
}


static void WriteDbgDrawTriangleDescriptorSet()
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DBG_DRAW_TRIANGLES, DBG_DRAW_TRIANGLES_VERTEX_BUFFER_DESCRIPTOR_SLOT, 0, s_dbgTriangleVertexDataGPU);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DBG_DRAW_TRIANGLES, DBG_DRAW_TRIANGLES_DATA_DESCRIPTOR_SLOT, 0, s_dbgTriangleDataGPU);
#endif
}


static void WriteDbgRTViewDescriptorSet()
{
#ifdef ENG_DEBUG_DRAW_ENABLED
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DBG_RT_VIEW, DBG_RT_VIEW_COMMON_DEPTH_DESCRIPTOR_SLOT, 0, s_depthRTColorView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DBG_RT_VIEW, DBG_RT_VIEW_COMMON_HZB_DESCRIPTOR_SLOT, 0, s_HZBView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DBG_RT_VIEW, DBG_RT_VIEW_GBUFFER_0_DESCRIPTOR_SLOT, 0, s_gbufferRTViews[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DBG_RT_VIEW, DBG_RT_VIEW_GBUFFER_1_DESCRIPTOR_SLOT, 0, s_gbufferRTViews[1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DBG_RT_VIEW, DBG_RT_VIEW_GBUFFER_2_DESCRIPTOR_SLOT, 0, s_gbufferRTViews[2], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DBG_RT_VIEW, DBG_RT_VIEW_GBUFFER_3_DESCRIPTOR_SLOT, 0, s_gbufferRTViews[3], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DBG_RT_VIEW, DBG_RT_VIEW_IRRADIANCE_MAP_DESCRIPTOR_SLOT, 0, s_irradianceMapTextureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DBG_RT_VIEW, DBG_RT_VIEW_PREFILTERED_ENV_MAP_DESCRIPTOR_SLOT, 0, s_prefilteredEnvMapTextureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DBG_RT_VIEW, DBG_RT_VIEW_BRDF_LUT_DESCRIPTOR_SLOT, 0, s_brdfLUTTextureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DBG_RT_VIEW, DBG_RT_VIEW_SKYBOX_DESCRIPTOR_SLOT, 0, s_skyboxTextureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DBG_RT_VIEW, DBG_RT_VIEW_CSM_DESCRIPTOR_SLOT, 0, s_csmRTViewArray, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    s_descriptorBuffer.WriteDescriptor(DESC_SET_ID_DBG_RT_VIEW, DBG_RT_VIEW_COLOR_16F_DESCRIPTOR_SLOT, 0, s_colorRTView16F, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
#endif
}


static void WriteDescriptorSets()
{
    WriteCommonDescriptorSet();

    WriteGeomCullingDescriptorSet();
    WriteGeomBatchingDescriptorSet();
    WriteGeomDrawCmdGenDescriptorSet();
    
    WriteZPassDescriptorSet();
    
    WriteCSMGeomCullingDescriptorSet();
    WriteCSMGeomBatchingDescriptorSet();
    WriteCSMGeomDrawCmdGenDescriptorSet();
    WriteHZBGenDescriptorSets();

    WriteCSMRenderDescriptorSet();
    
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
    WriteDbgRTViewDescriptorSet();
}


static void LoadSceneMeshInstData(const gltf::Asset& asset, const gltf::Mesh& mesh, size_t primIdx)
{
    ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::DarkMagenta, "Load_Scene_Mesh_Data_%s", mesh.name.c_str());

    auto GetVertexAttribAccessor = [](const gltf::Asset& asset, const gltf::Primitive& primitive, std::string_view name) -> const gltf::Accessor*
    {
        const fastgltf::Attribute* pAttrib = primitive.findAttribute(name); 
        return pAttrib != primitive.attributes.cend() ? &asset.accessors[pAttrib->accessorIndex] : nullptr;
    }; 

    const gltf::Primitive& primitive = mesh.primitives[primIdx];

    CORE_ASSERT_MSG(primitive.indicesAccessor.has_value(), "%zu primitive of %s mesh doesn't contation index accessor", primIdx, mesh.name.c_str());
    const gltf::Accessor& indexAccessor = asset.accessors[primitive.indicesAccessor.value()];

    std::vector<IndexType> indices(indexAccessor.count);
    gltf::copyFromAccessor<IndexType>(asset, indexAccessor, indices.data());

    const gltf::Accessor* pPosAccessor = GetVertexAttribAccessor(asset, primitive, "POSITION");
    CORE_ASSERT_MSG(pPosAccessor != nullptr, "Failed to find POSITION vertex attribute accessor for %zu primitive of %s mesh", primIdx, mesh.name.c_str());

    std::vector<glm::float3> positions(pPosAccessor->count);
    gltf::copyFromAccessor<glm::float3>(asset, *pPosAccessor, positions.data());

    const gltf::Accessor* pNormAccessor = GetVertexAttribAccessor(asset, primitive, "NORMAL");
    CORE_ASSERT_MSG(pNormAccessor != nullptr, "Failed to find NORMAL vertex attribute accessor for %zu primitive of %s mesh", primIdx, mesh.name.c_str());
    CORE_ASSERT(pPosAccessor->count == pNormAccessor->count);
    
    std::vector<glm::float3> normals(pNormAccessor->count);
    gltf::copyFromAccessor<glm::float3>(asset, *pNormAccessor, normals.data());

    const gltf::Accessor* pUvAccessor = GetVertexAttribAccessor(asset, primitive, "TEXCOORD_0");
    CORE_ASSERT_MSG(pUvAccessor != nullptr, "Failed to find TEXCOORD_0 vertex attribute accessor for %zu primitive of %s mesh", primIdx, mesh.name.c_str());
    CORE_ASSERT(pPosAccessor->count == pUvAccessor->count);

    std::vector<glm::float2> uvs(pUvAccessor->count);
    gltf::copyFromAccessor<glm::float2>(asset, *pUvAccessor, uvs.data());

    const gltf::Accessor* pTangAccessor = GetVertexAttribAccessor(asset, primitive, "TANGENT");

    std::vector<glm::float4> tangents;

    if (pTangAccessor) {
        CORE_ASSERT(pPosAccessor->count == pTangAccessor->count);

        tangents.resize(pTangAccessor->count);
        gltf::copyFromAccessor<glm::float4>(asset, *pTangAccessor, tangents.data());
    } else {
        CORE_LOG_WARN("Failed to find TANGENT vertex attribute accessor for %zu primitive of %s mesh. Using runtime computed tangents", primIdx, mesh.name.c_str());

        tangents.resize(normals.size());

        for (size_t i = 0; i < tangents.size(); ++i) {
            const glm::float3& lnorm = normals[i];
            const glm::float3 binorm = math::IsZero(glm::dot(lnorm, -M3D_AXIS_Z)) ? -M3D_AXIS_Z : -M3D_AXIS_Y;
            tangents[i] = glm::float4(glm::normalize(glm::cross(lnorm, binorm)), 1.f);
        }
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

    const size_t currVertCount = s_cpuGeomStreamBuffers[COMMON_GEOM_STREAM_POSITION].size() / 2; // position is packed 2 x uint

    COMMON_MESH cpuMesh = {};
    
    cpuMesh.FIRST_VERTEX = currVertCount;
    cpuMesh.VERTEX_COUNT = positions.size();
    cpuMesh.FIRST_LOD = s_cpuMeshLODData.size();

    {
        ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::DarkMagenta, "Mesh_%s_LOD_Generation", mesh.name.c_str());

        std::vector<IndexType> currLodIndices = indices;
        std::vector<IndexType> nextLodIndices(currLodIndices.size());

        for (size_t i = 0; i < MAX_GEOM_LOD_COUNT; ++i) {
            COMMON_MESH_LOD lod = {};
            lod.FIRST_INDEX = s_cpuGeomIndexBuffer.size();
            lod.INDEX_COUNT = currLodIndices.size();

            CORE_LOG_TRACE("Mesh %s LOD %zu: index count: %u", mesh.name.c_str(), i, lod.INDEX_COUNT);
            
            s_cpuMeshLODData.emplace_back(lod);
    
            ++cpuMesh.LOD_COUNT;
            
            s_cpuGeomIndexBuffer.reserve(s_cpuGeomIndexBuffer.size() + currLodIndices.size());
    
            for (IndexType index : currLodIndices) {
                s_cpuGeomIndexBuffer.emplace_back(cpuMesh.FIRST_VERTEX + index);
            }
    
            const size_t nextLodIndexCountTarget = (size_t)(((float)currLodIndices.size() * LOD_SIMPLIFICATION_COEF) + 2) / 3 * 3;
    
            const size_t nextLodIndexCount = meshopt_simplify(
                nextLodIndices.data(), 
                currLodIndices.data(), 
                currLodIndices.size(), 
                &positions[0].x, 
                positions.size(), 
                sizeof(glm::float3), 
                nextLodIndexCountTarget, 
                LOD_SIMPLIFICATION_ERROR
            );
    
            CORE_ASSERT(nextLodIndexCount <= currLodIndices.size());
    
            if (nextLodIndexCount == currLodIndices.size()) {
                break;
            }
    
            nextLodIndices.resize(nextLodIndexCount);
            currLodIndices.swap(nextLodIndices);
        }
    }

    cpuMesh.PackAABB_LCS(minVert, maxVert + M3D_EPS);

    s_cpuMeshData.emplace_back(cpuMesh);

    static constexpr size_t POS_SIZE_UI = 2;
    
    std::vector<uint32_t>& posStream = s_cpuGeomStreamBuffers[COMMON_GEOM_STREAM_POSITION];
    posStream.reserve(posStream.size() + positions.size() * POS_SIZE_UI);

    for (const glm::float3& pos : positions) {
        posStream.emplace_back(glm::packHalf2x16(glm::float2(pos.x, pos.y)));
        posStream.emplace_back(glm::packHalf2x16(glm::float2(pos.z, 0.f)));
    }

    std::vector<uint32_t>& normStream = s_cpuGeomStreamBuffers[COMMON_GEOM_STREAM_NORMAL];
    normStream.reserve(normStream.size() + normals.size());

    for (const glm::float3& normal : normals) {
        normStream.emplace_back(glm::packSnorm4x8(glm::float4(normal.x, normal.y, normal.z, 0.f)));
    }

    std::vector<uint32_t>& uvStream = s_cpuGeomStreamBuffers[COMMON_GEOM_STREAM_UV];
    uvStream.reserve(uvStream.size() + uvs.size());

    for (const glm::float2& uv : uvs) {
        uvStream.emplace_back(glm::packHalf2x16(uv));
    }

    std::vector<uint32_t>& tangStream = s_cpuGeomStreamBuffers[COMMON_GEOM_STREAM_TANGENT];
    tangStream.reserve(tangStream.size() + tangents.size());

    for (const glm::float4& tang : tangents) {
        tangStream.emplace_back(glm::packSnorm4x8(glm::float4(tang.x, tang.y, tang.z, tang.w > 0.f ? 1.f : -1.f)));
    }
}


static void LoadSceneMeshData(const gltf::Asset& asset)
{
    ENG_PROFILE_SCOPED_MARKER_C("Load_Scene_Mesh_Data", eng::ProfileColor::DarkMagenta);

    eng::Timer timer;

    for (const gltf::Mesh& mesh : asset.meshes) {
        for (size_t primIdx = 0; primIdx < mesh.primitives.size(); ++primIdx) {
            LoadSceneMeshInstData(asset, mesh, primIdx);
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
        
        mtl.ALBEDO_MULT.x = pbrData.baseColorFactor.x();
        mtl.ALBEDO_MULT.y = pbrData.baseColorFactor.y();
        mtl.ALBEDO_MULT.z = pbrData.baseColorFactor.z();
        mtl.ALBEDO_MULT.w = pbrData.baseColorFactor.w();
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

        mtl.EMISSIVE_MULT.x = material.emissiveFactor.x();
        mtl.EMISSIVE_MULT.y = material.emissiveFactor.y();
        mtl.EMISSIVE_MULT.z = material.emissiveFactor.z();

        mtl.FLAGS = 0;
        mtl.FLAGS |= (material.doubleSided ? glm::uint(COMMON_MATERIAL_FLAGS::DOUBLE_SIDED) : glm::uint(0));

        if (material.alphaMode == gltf::AlphaMode::Mask) {
            mtl.FLAGS |= glm::uint(COMMON_MATERIAL_FLAGS::ALPHA_KILL);
        } else if (material.alphaMode == gltf::AlphaMode::Blend) {
            CORE_LOG_WARN("Materials %s is transparent which is not supported yet", material.name.c_str());
            // mtl.FLAGS |= glm::uint(COMMON_MATERIAL_FLAGS::ALPHA_BLEND);
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

    auto GetInstAABB = [&](uint32_t meshIdx, const glm::float4x4& wMatr) -> math::AABB
    {
        const COMMON_MESH& mesh = s_cpuMeshData[meshIdx];
        
        return GetWorldAABB(mesh.GetAABB_LCS(), wMatr);
    };

    for (size_t sceneId = 0; sceneId < asset.scenes.size(); ++sceneId) {
        gltf::iterateSceneNodes(asset, sceneId, gltf::math::fmat4x4(1.f), [&](auto&& node, auto&& trs)
        {
            static_assert(sizeof(trs) == sizeof(glm::float4x4));
    
            glm::float4x4 transform(1.f);
            memcpy(&transform, &trs, sizeof(transform));
    
            if (node.meshIndex.has_value()) {                
                const uint32_t meshIdx = node.meshIndex.value();
                const gltf::Mesh& mesh = asset.meshes[meshIdx];

                const uint32_t baseIdx = meshBaseIdxOffsets[meshIdx];

                for (uint32_t i = 0; i < mesh.primitives.size(); ++i) {
                    const gltf::Primitive& primitive = mesh.primitives[i];

                    COMMON_INST inst = {};

                    inst.MESH_IDX = baseIdx + i;

                    CORE_ASSERT_MSG(primitive.materialIndex.has_value(), "Some of mesh %s primitive doesn't have material", mesh.name.c_str());
                    inst.MATERIAL_IDX = primitive.materialIndex.value();

                    inst.MATR_WCS = glm::transpose(transform);
                    inst.PackAABB_WCS(GetInstAABB(inst.MESH_IDX, transform));

                    s_cpuInstData.emplace_back(inst);
                }
            } else if (!s_mainCameraLoaded && node.cameraIndex.has_value()) {
                const gltf::Camera& camera = asset.cameras[node.cameraIndex.value()];

                s_mainCamera.SetTransform(transform);

                if (std::holds_alternative<gltf::Camera::Perspective>(camera.camera)) {
                    const gltf::Camera::Perspective& proj = std::get<gltf::Camera::Perspective>(camera.camera);

                    s_mainCamera.SetPerspProjection(proj.yfov, (float)s_pWnd->GetWidth() / s_pWnd->GetHeight(), proj.znear, 
                        proj.zfar.has_value() ? proj.zfar.value() : CAMERA_ZFAR);
                } else {
                    const gltf::Camera::Orthographic& proj = std::get<gltf::Camera::Orthographic>(camera.camera);

                    s_mainCamera.SetOrthoProjection(-proj.xmag, proj.xmag, -proj.ymag, proj.ymag, proj.znear, proj.zfar);
                }

                s_mainCameraLoaded = true;
            }
        });
    }

    CORE_LOG_INFO("FastGLTF: Instance data loading finished: %f ms", timer.End().GetDuration<float, std::milli>());

    timer.Reset().Start();

    std::sort(s_cpuInstData.begin(), s_cpuInstData.end(), 
        [](const COMMON_INST& a, const COMMON_INST& b) {
            return a.MESH_IDX < b.MESH_IDX;
        }
    );

    CORE_LOG_INFO("Instance data sorting finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void UploadGPUGeomStream(COMMON_GEOM_STREAM ID)
{
    const size_t gpuStreamSize = s_cpuGeomStreamBuffers[ID].size() * sizeof(uint32_t);
    CORE_ASSERT(gpuStreamSize <= s_commonStagingBuffer.GetMemorySize());

    void* pDataGPU = s_commonStagingBuffer.Map();
    memcpy(pDataGPU, s_cpuGeomStreamBuffers[ID].data(), gpuStreamSize);
    s_commonStagingBuffer.Unmap();

    vkn::AllocationInfo streamBufAllocInfo = {};
    streamBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    streamBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    s_geomStreamBuffers[ID].Create(&s_vkDevice, gpuStreamSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, streamBufAllocInfo);
    s_vkDevice.SetObjDebugName(s_geomStreamBuffers[ID], "COMMON_GEOM_STREAM_%s", COMMON_GEOM_STREAM_DBG_NAMES[ID]);

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        cmdBuffer.CmdCopyBuffer(s_commonStagingBuffer, s_geomStreamBuffers[ID], gpuStreamSize); 
    });
}


static void UploadGPUMeshData()
{
    ENG_PROFILE_SCOPED_MARKER_C("Upload_GPU_Mesh_Data", eng::ProfileColor::DarkMagenta);

    eng::Timer timer;

    for (size_t i = 0; i < COMMON_GEOM_STREAM_COUNT; ++i) {
        UploadGPUGeomStream(static_cast<COMMON_GEOM_STREAM>(i));
    }

    const size_t gpuIndexBufferSize = s_cpuGeomIndexBuffer.size() * sizeof(IndexType);
    CORE_ASSERT(gpuIndexBufferSize <= s_commonStagingBuffer.GetMemorySize());

    void* pIndexBufferData = s_commonStagingBuffer.Map();
    memcpy(pIndexBufferData, s_cpuGeomIndexBuffer.data(), gpuIndexBufferSize);
    s_commonStagingBuffer.Unmap();

    vkn::AllocationInfo idxBufAllocInfo = {};
    idxBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    idxBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    s_geomIndexBuffer.Create(&s_vkDevice, gpuIndexBufferSize, VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, idxBufAllocInfo);
    s_vkDevice.SetObjDebugName(s_geomIndexBuffer, "COMMON_IB");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        cmdBuffer.CmdCopyBuffer(s_commonStagingBuffer, s_geomIndexBuffer, gpuIndexBufferSize);    
    });

    const size_t meshDataBufferSize = s_cpuMeshData.size() * sizeof(COMMON_MESH);
    CORE_ASSERT(meshDataBufferSize <= s_commonStagingBuffer.GetMemorySize());

    void* pMeshBufferData = s_commonStagingBuffer.Map();
    memcpy(pMeshBufferData, s_cpuMeshData.data(), meshDataBufferSize);
    s_commonStagingBuffer.Unmap();

    vkn::AllocationInfo meshInfosBufAllocInfo = {};
    meshInfosBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    meshInfosBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    
    s_commonMeshBuffer.Create(&s_vkDevice, meshDataBufferSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, meshInfosBufAllocInfo);
    s_vkDevice.SetObjDebugName(s_commonMeshBuffer, "COMMON_MESH_BUFFER");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        cmdBuffer.CmdCopyBuffer(s_commonStagingBuffer, s_commonMeshBuffer, meshDataBufferSize);
    });

    const size_t meshLODDataBufferSize = s_cpuMeshLODData.size() * sizeof(COMMON_MESH_LOD);
    CORE_ASSERT(meshLODDataBufferSize <= s_commonStagingBuffer.GetMemorySize());

    void* pMeshLODBufferData = s_commonStagingBuffer.Map();
    memcpy(pMeshLODBufferData, s_cpuMeshLODData.data(), meshLODDataBufferSize);
    s_commonStagingBuffer.Unmap();

    vkn::AllocationInfo meshLODInfosBufAllocInfo = {};
    meshLODInfosBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    meshLODInfosBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    
    s_commonMeshLODBuffer.Create(&s_vkDevice, meshLODDataBufferSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, meshLODInfosBufAllocInfo);
    s_vkDevice.SetObjDebugName(s_commonMeshLODBuffer, "COMMON_MESH_LOD_BUFFER");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        cmdBuffer.CmdCopyBuffer(s_commonStagingBuffer, s_commonMeshLODBuffer, meshLODDataBufferSize);
    });

    CORE_LOG_INFO("FastGLTF: Mesh data GPU upload finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void UploadGPUTextureData()
{
    ENG_PROFILE_SCOPED_MARKER_C("Upload_GPU_Texture_Data", eng::ProfileColor::DarkMagenta);

    eng::Timer timer;

    s_commonMaterialTextures.resize(s_cpuTexturesData.size());
    s_commonMaterialTextureViews.resize(s_cpuTexturesData.size());

    for (size_t i = 0; i < s_cpuTexturesData.size(); ++i) {
        const size_t textureIdx = i;

        const TextureLoadData& texData = s_cpuTexturesData[textureIdx];
        const size_t texSizeInBytes = texData.GetMemorySize();

        CORE_ASSERT(texSizeInBytes <= s_commonStagingBuffer.GetMemorySize());

        void* pImageData = s_commonStagingBuffer.Map();
        memcpy(pImageData, texData.GetData(), texSizeInBytes);
        s_commonStagingBuffer.Unmap();

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
        sceneImage.Create(imageCreateInfo);
        s_vkDevice.SetObjDebugName(sceneImage, "COMMON_MTL_TEXTURE_%zu", textureIdx);

        VkComponentMapping mapping = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        vkn::TextureView& sceneImageView = s_commonMaterialTextureViews[textureIdx];

        sceneImageView.Create(sceneImage, mapping, subresourceRange);
        s_vkDevice.SetObjDebugName(sceneImageView, "COMMON_MTL_TEXTURE_VIEW_%zu", textureIdx);

        ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
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

            cmdBuffer.CmdCopyBuffer(s_commonStagingBuffer, texture, copyInfo);

            const TextureLoadData& texData = s_cpuTexturesData[textureIdx];

            GenerateTextureMipmaps(cmdBuffer, texture, texData);

            cmdBuffer
                .BeginBarrierList()
                    .AddTextureBarrier(texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, 
                        VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
                .Push();
        });
    }
}


static void UploadGPUMaterialData()
{
    ENG_PROFILE_SCOPED_MARKER_C("Upload_GPU_Material_Data", eng::ProfileColor::DarkMagenta);

    eng::Timer timer;

    const size_t mtlDataBufferSize = s_cpuMaterialData.size() * sizeof(COMMON_MATERIAL);
    CORE_ASSERT(mtlDataBufferSize <= s_commonStagingBuffer.GetMemorySize());

    void* pData = s_commonStagingBuffer.Map();
    memcpy(pData, s_cpuMaterialData.data(), mtlDataBufferSize);
    s_commonStagingBuffer.Unmap();

    vkn::AllocationInfo mtlBufAllocInfo = {};
    mtlBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    mtlBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    s_commonMaterialBuffer.Create(&s_vkDevice, mtlDataBufferSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, mtlBufAllocInfo);
    s_vkDevice.SetObjDebugName(s_commonMaterialBuffer, "COMMON_MATERIAL_DATA");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer) {
        cmdBuffer.CmdCopyBuffer(s_commonStagingBuffer, s_commonMaterialBuffer, mtlDataBufferSize);
    });

    CORE_LOG_INFO("FastGLTF: Material data GPU upload finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void UploadGPUInstData()
{
    ENG_PROFILE_SCOPED_MARKER_C("Upload_GPU_Inst_Data", eng::ProfileColor::DarkMagenta);

    eng::Timer timer;

    const size_t instBufferSize = s_cpuInstData.size() * sizeof(COMMON_INST);
    CORE_ASSERT(instBufferSize <= s_commonStagingBuffer.GetMemorySize());

    {
        void* pData = s_commonStagingBuffer.Map();
        memcpy(pData, s_cpuInstData.data(), instBufferSize);
        s_commonStagingBuffer.Unmap();
    }

    vkn::AllocationInfo instInfosBufAllocInfo = {};
    instInfosBufAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    instInfosBufAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    s_commonInstBuffer.Create(&s_vkDevice, instBufferSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, instInfosBufAllocInfo);
    s_vkDevice.SetObjDebugName(s_commonInstBuffer, "COMMON_INSTANCE_BUFFER");

    ImmediateSubmitQueue(s_vkDevice.GetQueue(), [&](vkn::CmdBuffer& cmdBuffer){
        cmdBuffer.CmdCopyBuffer(s_commonStagingBuffer, s_commonInstBuffer, instBufferSize);
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

    if (!s_mainCameraLoaded) {
        s_mainCamera.SetPosition(glm::float3(0.f, 0.f, 16.f));
        s_mainCamera.SetRotation(glm::quatLookAt(-M3D_AXIS_Z, M3D_AXIS_Y));
        s_mainCamera.SetPerspProjection(glm::radians(90.f), (float)s_pWnd->GetWidth() / s_pWnd->GetHeight(), CAMERA_ZNEAR, CAMERA_ZFAR);
    }

    s_mainCamera.Update();

    CORE_LOG_INFO("\"%s\" loading finished: %f ms", filepath.filename().string().c_str(), timer.End().GetDuration<float, std::milli>());
}


static void CreateCommonConstBuffer()
{
    s_commonConstBuffer.CreateConstBuffer(&s_vkDevice, sizeof(COMMON_CB_DATA));
    s_vkDevice.SetObjDebugName(s_commonConstBuffer, "COMMON_CB");
}


static void CreateCommonDbgConstBuffer()
{
#ifndef ENG_BUILD_RELEASE
    s_commonDbgConstBuffer.CreateConstBuffer(&s_vkDevice, sizeof(COMMON_DBG_CB_DATA));
    s_vkDevice.SetObjDebugName(s_commonDbgConstBuffer, "COMMON_DBG_CB");
#endif
}


void UpdateGPUCommonConstBuffer()
{
    ENG_PROFILE_SCOPED_MARKER_C("Update_Common_Const_Buffer", eng::ProfileColor::Cyan4);

    COMMON_CB_DATA& constBuff = *reinterpret_cast<COMMON_CB_DATA*>(s_commonConstBuffer.Map());

    for (size_t i = 0; i < COMMON_CSM_CASCADE_COUNT; ++i) {
        const eng::Camera& cam = s_csmCameras[i];

        constBuff.CSM_VIEW_FRUSTUMS[i]      = CopyCPUFrustumToGPU(cam.GetFrustum());
        constBuff.CSM_VIEW_MATRICES[i]      = cam.GetViewMatrix();
        constBuff.CSM_VIEW_PROJ_MATRICES[i] = cam.GetViewProjMatrix();
        constBuff.CSM_CASCADE_DISTANCES[i]  = CSM_CASCADE_DISTANCES[i];
    }

    const glm::float4x4& viewMatrix = s_mainCamera.GetViewMatrix();
    const glm::float4x4& projMatrix = s_mainCamera.GetProjMatrix();
    const glm::float4x4& viewProjMatrix = s_mainCamera.GetViewProjMatrix();

    constBuff.VIEW_MATRIX = viewMatrix;
    constBuff.PROJ_MATRIX = projMatrix;
    constBuff.VIEW_PROJ_MATRIX = viewProjMatrix;

    constBuff.INV_VIEW_MATRIX = glm::inverse(viewMatrix);
    constBuff.INV_PROJ_MATRIX = glm::inverse(projMatrix);
    constBuff.INV_VIEW_PROJ_MATRIX = glm::inverse(viewProjMatrix);

    const math::Frustum& camFrustum = s_mainCamera.GetFrustum();

    const FRUSTUM frustumGPU = CopyCPUFrustumToGPU(camFrustum);

    constBuff.CAMERA_FRUSTUM = frustumGPU;

    if (s_cullingTestMode) {
        constBuff.CULLING_CAMERA_FRUSTUM = CopyCPUFrustumToGPU(s_fixedCamCullFrustum);
        constBuff.CULLING_VIEW_PROJ_MATRIX = s_fixedCamCullViewProjMatr;
    } else {
        constBuff.CULLING_CAMERA_FRUSTUM = frustumGPU;
        constBuff.CULLING_VIEW_PROJ_MATRIX = viewProjMatrix;
    }
    
    constBuff.SCREEN_SIZE.x = static_cast<float>(s_pWnd->GetWidth());
    constBuff.SCREEN_SIZE.y = static_cast<float>(s_pWnd->GetHeight());

    constBuff.Z_NEAR = s_mainCamera.GetZNear();
    constBuff.Z_FAR = s_mainCamera.GetZFar();
    
    constBuff.CAM_WPOS = glm::float4(s_mainCamera.GetPosition(), 0.f);

    s_commonConstBuffer.Unmap();
}


void UpdateGPUDbgConstBuffer()
{
#ifndef ENG_BUILD_RELEASE
    ENG_PROFILE_SCOPED_MARKER_C("Update_Common_Dbg_Const_Buffer", eng::ProfileColor::Cyan4);

    COMMON_DBG_CB_DATA& constBuff = *reinterpret_cast<COMMON_DBG_CB_DATA*>(s_commonDbgConstBuffer.Map());

    constBuff.FORCED_GEOM_LOD = s_forcedGeomLOD;

    uint32_t flags_0 = 0;

    switch (s_tonemappingPreset) {
        case TonemapPreset::ACES:                  flags_0 |= (1u << 0u); break;
        case TonemapPreset::REINHARD:              flags_0 |= (1u << 1u); break;
        case TonemapPreset::PARTIAL_UNCHARTED_2:   flags_0 |= (1u << 2u); break;
        case TonemapPreset::UNCHARTED_2:           flags_0 |= (1u << 3u); break;
    }

    flags_0 |= s_useIndirectLighting                       ? (1u << 4u) : 0;
    flags_0 |= s_useMeshCulling && s_useMeshFrustumCulling ? (1u << 5u) : 0;
    flags_0 |= s_useMeshCulling && s_useMeshHZBCulling     ? (1u << 6u) : 0;

    constBuff.FLAGS_0 = flags_0;
    constBuff.RT_VIEW_TYPE = s_dbgOutputRTType;

    s_commonDbgConstBuffer.Unmap();
#endif
}


static void ResizeVkSwapchain(eng::Window& window)
{
    bool resizeSucceded;
    s_vkSwapchain.Resize(window.GetWidth(), window.GetHeight(), resizeSucceded);
    
    s_swapchainRecreateRequired = !resizeSucceded;
}


static void UpdateMainCamera()
{
    const float moveDist = glm::length(s_mainCameraVel);

    if (!math::IsZero(moveDist)) {
        const glm::float3 moveDir = glm::normalize(s_mainCamera.GetRotation() * (s_mainCameraVel / moveDist));
        s_mainCamera.MoveAlongDir(moveDir, moveDist);
    }

    s_mainCamera.Update();
}


static void UpdateCSMDataCPU()
{
    ENG_PROFILE_SCOPED_MARKER_C("Update_CSM_Data_CPU", eng::ProfileColor::Cyan4);

    eng::Camera cascadeCamera = s_mainCamera;

    auto GetCascadeSphereVolumeRadius = [](std::span<const glm::float3> points, const glm::float3& center) {
        float radius = 0.f;

        for (const glm::float3& point : points) {
            radius = glm::max(radius, glm::distance(point, center));
        }

        return radius;
    };

    for (uint32_t i = 0; i < COMMON_CSM_CASCADE_COUNT; ++i) {
        const float zNear = i == 0 ? 0.01f : CSM_CASCADE_DISTANCES[i - 1];
        const float zFar = CSM_CASCADE_DISTANCES[i];

        cascadeCamera.SetZNearFar(zNear, zFar);
        cascadeCamera.Update();

        const math::Frustum& cascadeFrustum = cascadeCamera.GetFrustum();

        std::span<const glm::float3> frCorners = cascadeFrustum.GetPoints();
        
        glm::float3 frCenter = cascadeFrustum.GetCenter();
        const float frRadius = GetCascadeSphereVolumeRadius(frCorners, frCenter);

        glm::float3 lightPos = frCenter - SUN_LIGHT_DIR * frRadius;
        const glm::quat lightView = glm::quatLookAt(SUN_LIGHT_DIR, M3D_AXIS_Y);
        const glm::quat invLightView = glm::inverse(lightView);

        const float orthoSize = 2.f * frRadius;
        const float texelSize = orthoSize / CSM_CASCADE_RT_SIZE;

        glm::float3 frCenterLS = lightView * glm::float4(lightPos - frCenter, 1.f);
        frCenterLS.x = glm::round(frCenterLS.x / texelSize) * texelSize;
        frCenterLS.y = glm::round(frCenterLS.y / texelSize) * texelSize;

        frCenter = invLightView * glm::float4(frCenterLS, 1.f);
        lightPos = frCenter - SUN_LIGHT_DIR * frRadius;

        eng::Camera& csmCamera = s_csmCameras[i];

        csmCamera.SetPosition(lightPos);
        csmCamera.SetRotation(lightView);
        csmCamera.SetOrthoProjection(-frRadius, frRadius, -frRadius, frRadius, -SUN_DISTANCE, 3.f * frRadius);
    
        csmCamera.Update();
    }
}


static void UpdateScene()
{
    ENG_PROFILE_SCOPED_MARKER_C("Update_Scene", eng::ProfileColor::Cyan4);

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

    UpdateMainCamera();
    UpdateCSMDataCPU();

    if (s_drawInstAABBs) {
        const math::Frustum& frustum = s_mainCamera.GetFrustum();

        static constexpr glm::float4 COLOR = glm::float4(1.f, 1.f, 0.f, 1.f);

        for (const COMMON_INST& inst : s_cpuInstData) {
            const math::AABB aabb = inst.GetAABB_WCS(); 

            if (frustum.IsIntersect(aabb)) {
                RenderDebugAABBWired(math::MakeTS(aabb.GetCenter(), aabb.GetSize()), COLOR);
            }
        }
    }

    if (s_cullingTestMode) {
        RenderDebugFrustumFilled(s_fixedCamCullInvViewProjMatr, glm::float4(0.5f, 0.5f, 0.5f, 0.35f));
        RenderDebugFrustumWired(s_fixedCamCullInvViewProjMatr, glm::float4(1.f));
    }

    if (s_csmTestMode) {
        for (size_t i = 0; i < COMMON_CSM_CASCADE_COUNT; ++i) {
            RenderDebugFrustumFilled(s_fixedCamCsmInvViewProjMatr[i], CSM_CASCADE_COLORS[i]);
            RenderDebugFrustumWired(s_fixedCamCsmInvViewProjMatr[i], glm::float4(1.f));
        }
    }
}


static void PresentImage(uint32_t imageIndex)
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

    vkn::PSO& pso = s_PSOs[PASS_ID_IRRADIANCE_MAP_GEN];

    cmdBuffer.CmdBindPSO(pso);
    
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_IRRADIANCE_MAP_GEN, .shaderSetIdx = DESC_SET_PER_DRAW });

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

    vkn::PSO& pso = s_PSOs[PASS_ID_PREFILT_ENV_MAP_GEN];

    cmdBuffer.CmdBindPSO(pso);

    PREFILTERED_ENV_MAP_PER_DRAW_DATA pushConsts = {};
    pushConsts.ENV_MAP_FACE_SIZE.x = s_skyboxTexture.GetSizeX();
    pushConsts.ENV_MAP_FACE_SIZE.y = s_skyboxTexture.GetSizeY();

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_prefilteredEnvMapTexture, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
            .Push();

    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_PREFILT_ENV_MAP_GEN, .shaderSetIdx = DESC_SET_PER_DRAW });

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

    vkn::PSO& pso = s_PSOs[PASS_ID_BRDF_LUT_GEN];

    cmdBuffer.CmdBindPSO(pso);

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_brdfLUTTexture, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
        .Push();

    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_BRDF_LUT_GEN, .shaderSetIdx = DESC_SET_PER_DRAW });

    cmdBuffer.CmdDispatch((uint32_t)ceil(COMMON_BRDF_INTEGRATION_LUT_SIZE.x / 32.f), (uint32_t)ceil(COMMON_BRDF_INTEGRATION_LUT_SIZE.y / 32.f), 1u);

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_brdfLUTTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
        .Push();

    CORE_LOG_INFO("BRDF LUT generation finished: %f ms", timer.End().GetDuration<float, std::milli>());
}


static void GeomCullingClearCounters(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Geom_Culling_Clear_Counters", eng::ProfileColor::Black);

    vkn::BarrierList& barriers = cmdBuffer.BeginBarrierList();

    for (uint32_t phase = 0; phase < GEOM_CULLING_PHASES_COUNT; ++phase) {
        for (uint32_t queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            barriers.AddBufferBarrier(s_visGeomIDQueueSizeBuffers[phase][queue], VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
            barriers.AddBufferBarrier(s_geomBatchQueueSizeBuffers[phase][queue], VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
            barriers.AddBufferBarrier(s_sortedVisGeomIDQueueSizeBuffers[phase][queue], VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        }
    }

    barriers.Push();

    for (size_t phase = 0; phase < GEOM_CULLING_PHASES_COUNT; ++phase) {
        for (uint32_t queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            cmdBuffer.CmdFillBuffer(s_visGeomIDQueueSizeBuffers[phase][queue], 0, 0, sizeof(glm::uint));
            cmdBuffer.CmdFillBuffer(s_geomBatchQueueSizeBuffers[phase][queue], 0, 0, sizeof(glm::uint));
            cmdBuffer.CmdFillBuffer(s_sortedVisGeomIDQueueSizeBuffers[phase][queue], 0, 0, sizeof(glm::uint));
        }
    }
}


static void GeomCullingPass(vkn::CmdBuffer& cmdBuffer, uint32_t phase)
{
    CORE_ASSERT(phase < GEOM_CULLING_PHASES_COUNT);

    const PassID pass = phase == 0 ? PASS_ID_GEOM_CULLING_PHASE_1 : PASS_ID_GEOM_CULLING_PHASE_2;

    vkn::BarrierList& barriers = cmdBuffer.BeginBarrierList();

    for (uint32_t queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
        barriers.AddBufferBarrier(s_visGeomIDQueueBuffers[phase][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
        barriers.AddBufferBarrier(s_visGeomIDQueueSizeBuffers[phase][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
        
        barriers.AddBufferBarrier(s_geomBatchDispatchCmdBuffers[phase][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
    }

    if (phase == 0) {
        barriers.AddBufferBarrier(s_geomInstVisFlagsBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    } else {
        barriers
            .AddBufferBarrier(s_geomInstVisFlagsBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT)
            .AddTextureBarrier(s_HZB, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    barriers.Push();

    vkn::PSO& pso = s_PSOs[pass];

    cmdBuffer.CmdBindPSO(pso);
    
    const DescSetID passDescID = phase == 0 ? DESC_SET_ID_GEOM_CULLING_PHASE_1 : DESC_SET_ID_GEOM_CULLING_PHASE_2;

    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = passDescID, .shaderSetIdx = DESC_SET_PER_DRAW });

    GEOM_CULLING_PER_DRAW_DATA pushConsts = {};
    pushConsts.HZB_MIPS_COUNT = s_HZB.GetMipCount();

    cmdBuffer.CmdPushConstants(pso, VK_SHADER_STAGE_COMPUTE_BIT, pushConsts);

    cmdBuffer.CmdDispatch(ceil(s_cpuInstData.size() / (float)GEOM_CULLING_CS_GROUP_SIZE), 1, 1);
}


static void PrevFrameGeomVisIDBufferPass(vkn::CmdBuffer& cmdBuffer)
{
    static constexpr char* markerName = "Prev_Frame_Geom_Vis_ID_Buffer_Pass";

    ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::IndianRed1, markerName);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, markerName, eng::ProfileColor::IndianRed1);

    GeomCullingPass(cmdBuffer, 0);
}


static void ThisFrameGeomVisIDBufferPass(vkn::CmdBuffer& cmdBuffer)
{
    static constexpr char* markerName = "This_Frame_Geom_Vis_ID_Buffer_Pass";

    ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::IndianRed1, markerName);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, markerName, eng::ProfileColor::IndianRed1);

    GeomCullingPass(cmdBuffer, 1);
}


static void GeomBatchingPass(vkn::CmdBuffer& cmdBuffer, uint32_t phase, GEOM_QUEUE queue)
{
    CORE_ASSERT(phase < GEOM_CULLING_PHASES_COUNT);
    CORE_ASSERT(queue < GEOM_QUEUE_COUNT);
    
    DescSetID setID;

    switch(queue) {
        case GEOM_QUEUE_OPAQUE:
            setID = phase == 0 ? DESC_SET_ID_GEOM_BATCHING_OPAQUE_PHASE_1 : DESC_SET_ID_GEOM_BATCHING_OPAQUE_PHASE_2;
            break;
        case GEOM_QUEUE_AKILL:
            setID = phase == 0 ? DESC_SET_ID_GEOM_BATCHING_AKILL_PHASE_1 : DESC_SET_ID_GEOM_BATCHING_AKILL_PHASE_2;
            break;
    }

    cmdBuffer
        .BeginBarrierList()
            .AddBufferBarrier(s_visGeomIDQueueBuffers[phase][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT)
            .AddBufferBarrier(s_visGeomIDQueueSizeBuffers[phase][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT)
            .AddBufferBarrier(s_geomBatchQueueBuffers[phase][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
            .AddBufferBarrier(s_geomBatchQueueSizeBuffers[phase][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
            .AddBufferBarrier(s_sortedVisGeomIDQueueBuffers[phase][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
            .AddBufferBarrier(s_sortedVisGeomIDQueueSizeBuffers[phase][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
            .AddBufferBarrier(s_geomDrawCmdGenDispatchCmdBuffers[phase][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
            
            .AddBufferBarrier(s_geomBatchDispatchCmdBuffers[phase][queue], VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT)
        .Push();

    vkn::PSO& pso = s_PSOs[PASS_ID_GEOM_BATCHING];

    cmdBuffer.CmdBindPSO(pso);

    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = (uint32_t)setID, .shaderSetIdx = DESC_SET_PER_DRAW });

    // GEOM_BATCH_PER_DRAW_DATA pushConsts = {};
    // cmdBuffer.CmdPushConstants(pso, VK_SHADER_STAGE_COMPUTE_BIT, pushConsts);

    cmdBuffer.CmdDispatchIndirect(s_geomBatchDispatchCmdBuffers[phase][queue]);
}


static void PrevFrameVisOccludersBatchingPass(vkn::CmdBuffer& cmdBuffer)
{
    {
        static constexpr char* markerName = "Prev_Frame_Vis_Occluders_Batching_Pass";

        ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::LightSteelBlue1, markerName);
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, markerName, eng::ProfileColor::LightSteelBlue1);

        {
            ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::LightSteelBlue1, "Prev_Frame_Vis_Occluders_Batching_Pass_Opaque");
            ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Prev_Frame_Vis_Occluders_Batching_Pass_Opaque", eng::ProfileColor::LightSteelBlue1);
    
            GeomBatchingPass(cmdBuffer, 0, GEOM_QUEUE_OPAQUE);
        }
    
        {
            ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::LightSteelBlue1, "Prev_Frame_Vis_Occluders_Batching_Pass_AKill");
            ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Prev_Frame_Vis_Occluders_Batching_Pass_AKill", eng::ProfileColor::LightSteelBlue1);
    
            GeomBatchingPass(cmdBuffer, 0, GEOM_QUEUE_AKILL);
        }
    }
}


static void ThisFrameVisGeometryBatchingPass(vkn::CmdBuffer& cmdBuffer)
{
    {
        static constexpr char* markerName = "This_Frame_Vis_Geometry_Batching_Pass";

        ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::LightSteelBlue1, markerName);
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, markerName, eng::ProfileColor::LightSteelBlue1);

        {
            ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::LightSteelBlue1, "This_Frame_Vis_Geometry_Batching_Pass_Opaque");
            ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "This_Frame_Vis_Geometry_Batching_Pass_Opaque", eng::ProfileColor::LightSteelBlue1);
    
            GeomBatchingPass(cmdBuffer, 1, GEOM_QUEUE_OPAQUE);
        }
    
        {
            ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::LightSteelBlue1, "This_Frame_Vis_Geometry_Batching_Pass_AKill");
            ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "This_Frame_Vis_Geometry_Batching_Pass_AKill", eng::ProfileColor::LightSteelBlue1);
    
            GeomBatchingPass(cmdBuffer, 1, GEOM_QUEUE_AKILL);
        }
    }
}


static void GeomDrawCmdGenPass(vkn::CmdBuffer& cmdBuffer, uint32_t phase, GEOM_QUEUE queue)
{
    CORE_ASSERT(phase < GEOM_CULLING_PHASES_COUNT);
    CORE_ASSERT(queue < GEOM_QUEUE_COUNT);
    
    DescSetID setID;

    switch(queue) {
        case GEOM_QUEUE_OPAQUE:
            setID = phase == 0 ? DESC_SET_ID_GEOM_DRAW_CMD_GEN_OPAQUE_PHASE_1 : DESC_SET_ID_GEOM_DRAW_CMD_GEN_OPAQUE_PHASE_2;
            break;
        case GEOM_QUEUE_AKILL:
            setID = phase == 0 ? DESC_SET_ID_GEOM_DRAW_CMD_GEN_AKILL_PHASE_1 : DESC_SET_ID_GEOM_DRAW_CMD_GEN_AKILL_PHASE_2;
            break;
    }

    cmdBuffer
        .BeginBarrierList()
            .AddBufferBarrier(s_geomBatchQueueBuffers[phase][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT)
            .AddBufferBarrier(s_geomBatchQueueSizeBuffers[phase][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT)
            .AddBufferBarrier(s_geomDrawCmdQueueBuffers[phase][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)

            .AddBufferBarrier(s_geomDrawCmdGenDispatchCmdBuffers[phase][queue], VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT)
        .Push();

    vkn::PSO& pso = s_PSOs[PASS_ID_GEOM_DRAW_CMD_GEN];

    cmdBuffer.CmdBindPSO(pso);

    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = setID, .shaderSetIdx = DESC_SET_PER_DRAW });

    // GEOM_DRAW_CMD_GEN_PER_DRAW_DATA pushConsts = {};
    // cmdBuffer.CmdPushConstants(pso, VK_SHADER_STAGE_COMPUTE_BIT, pushConsts);

    cmdBuffer.CmdDispatchIndirect(s_geomDrawCmdGenDispatchCmdBuffers[phase][queue]);
}


static void PrevFrameOccludersDrawCmdGenPass(vkn::CmdBuffer& cmdBuffer)
{
    {
        static constexpr char* markerName = "Prev_Frame_Occluders_Draw_Cmd_Gen_Pass";

        ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::ForestGreen, markerName);
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, markerName, eng::ProfileColor::ForestGreen);

        {
            ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::ForestGreen, "Prev_Frame_Occluders_Draw_Cmd_Gen_Pass_Opaque");
            ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Prev_Frame_Occluders_Draw_Cmd_Gen_Pass_Opaque", eng::ProfileColor::ForestGreen);
    
            GeomDrawCmdGenPass(cmdBuffer, 0, GEOM_QUEUE_OPAQUE);
        }
    
        {
            ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::ForestGreen, "Prev_Frame_Occluders_Draw_Cmd_Gen_Pass_AKill");
            ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Prev_Frame_Occluders_Draw_Cmd_Gen_Pass_AKill", eng::ProfileColor::ForestGreen);
    
            GeomDrawCmdGenPass(cmdBuffer, 0, GEOM_QUEUE_AKILL);
        }
    }
}


static void ThisFrameGeometryDrawCmdGenPass(vkn::CmdBuffer& cmdBuffer)
{
    {
        static constexpr char* markerName = "This_Frame_Geometry_Draw_Cmd_Gen_Pass";

        ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::ForestGreen, markerName);
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, markerName, eng::ProfileColor::ForestGreen);

        {
            ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::ForestGreen, "This_Frame_Geometry_Draw_Cmd_Gen_Pass_Opaque");
            ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "This_Frame_Geometry_Draw_Cmd_Gen_Pass_Opaque", eng::ProfileColor::ForestGreen);
    
            GeomDrawCmdGenPass(cmdBuffer, 1, GEOM_QUEUE_OPAQUE);
        }
    
        {
            ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::ForestGreen, "This_Frame_Geometry_Draw_Cmd_Gen_Pass_AKill");
            ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "This_Frame_Geometry_Draw_Cmd_Gen_Pass_AKill", eng::ProfileColor::ForestGreen);
    
            GeomDrawCmdGenPass(cmdBuffer, 1, GEOM_QUEUE_AKILL);
        }
    }
}


static void PrevFrameOccludersCullingPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::DodgerBlue1, "Prev_Frame_Occluders_Culling_Pass");
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Prev_Frame_Occluders_Culling_Pass", eng::ProfileColor::DodgerBlue1);

    GeomCullingClearCounters(cmdBuffer);

    PrevFrameGeomVisIDBufferPass(cmdBuffer);
    PrevFrameVisOccludersBatchingPass(cmdBuffer);
    PrevFrameOccludersDrawCmdGenPass(cmdBuffer);
}


static void ThisFrameGeometryCullingPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::DodgerBlue1, "This_Frame_Geometry_Culling_Pass");
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "This_Frame_Geometry_Culling_Pass", eng::ProfileColor::DodgerBlue1);

    ThisFrameGeomVisIDBufferPass(cmdBuffer);
    ThisFrameVisGeometryBatchingPass(cmdBuffer);
    ThisFrameGeometryDrawCmdGenPass(cmdBuffer);
}


void RenderPass_Depth(vkn::CmdBuffer& cmdBuffer, uint32_t phase, GEOM_QUEUE queue)
{
    CORE_ASSERT(phase < GEOM_CULLING_PHASES_COUNT);
    CORE_ASSERT(queue < GEOM_QUEUE_COUNT);
    
    DescSetID setID;

    switch(queue) {
        case GEOM_QUEUE_OPAQUE:
            setID = phase == 0 ? DESC_SET_ID_DEPTH_OPAQUE_PHASE_1 : DESC_SET_ID_DEPTH_OPAQUE_PHASE_2;
            break;
        case GEOM_QUEUE_AKILL:
            setID = phase == 0 ? DESC_SET_ID_DEPTH_AKILL_PHASE_1 : DESC_SET_ID_DEPTH_AKILL_PHASE_2;
            break;
    }
    
    const bool isAKillPass = queue == GEOM_QUEUE_AKILL;
    const bool needClearDepth = setID == DESC_SET_ID_DEPTH_OPAQUE_PHASE_1;

    vkn::Buffer& drawCmdBuffer = s_geomDrawCmdQueueBuffers[phase][queue];
    vkn::Buffer& drawCmdCountBuffer = s_geomBatchQueueSizeBuffers[phase][queue];
    vkn::Buffer& visIDBuffer = s_sortedVisGeomIDQueueBuffers[phase][queue];

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_depthRT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, 
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT)
            .AddBufferBarrier(drawCmdBuffer, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT)
            .AddBufferBarrier(visIDBuffer, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT)
        .Push();

    const VkExtent2D extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };

    vkn::RenderInfo renderInfo = {};

    renderInfo.renderArea.extent = extent;
    renderInfo.depthAttachment.view = &s_depthRTView;
    renderInfo.depthAttachment.loadOp  = needClearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    renderInfo.depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
#ifdef ENG_REVERSED_Z
    renderInfo.depthAttachment.clearValue.depthStencil.depth = 0.f;
#else
    renderInfo.depthAttachment.clearValue.depthStencil.depth = 1.f;
#endif

    cmdBuffer.CmdBeginRendering(renderInfo);
        cmdBuffer.CmdSetViewport(0.f, 0.f, extent.width, extent.height);
        cmdBuffer.CmdSetScissor(0, 0, extent.width, extent.height);

        vkn::PSO& pso = s_PSOs[PASS_ID_DEPTH];

        cmdBuffer.CmdBindPSO(pso);
        
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = (uint32_t)setID, .shaderSetIdx = DESC_SET_PER_DRAW });

        cmdBuffer.CmdBindIndexBuffer(s_geomIndexBuffer, 0, GetVkIndexType());

        ZPASS_PER_DRAW_DATA pushConsts = {};
        pushConsts.IS_AKILL_PASS = isAKillPass;

        cmdBuffer.CmdPushConstants(pso, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, pushConsts);

        cmdBuffer.CmdDrawIndexedIndirect(drawCmdBuffer, 0, drawCmdCountBuffer, 0, s_cpuInstData.size(), sizeof(COMMON_CMD_DRAW_INDEXED_INDIRECT));
    cmdBuffer.CmdEndRendering();

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_depthRT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, 
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_ASPECT_DEPTH_BIT)
        .Push();
}


void PrevFrameOccludersDepthPass(vkn::CmdBuffer& cmdBuffer)
{
    if (!s_useDepthPass) {
        return;
    }

    static constexpr const char* passLabelName = "Prev_Frame_Occluders_Depth_Pass";

    ENG_PROFILE_SCOPED_MARKER_C(passLabelName, eng::ProfileColor::Grey51);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, passLabelName, eng::ProfileColor::Grey51);

#ifndef ENG_BUILD_RELEASE
    SetWireframeMode(cmdBuffer, s_geomWireframeMode);
#endif

    {
        static constexpr const char* localPassLabelName = "Prev_Frame_Occluders_Depth_Pass_Opaque";

        ENG_PROFILE_SCOPED_MARKER_C("localPassLabelName", eng::ProfileColor::Grey51);
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, localPassLabelName, eng::ProfileColor::Grey51);
        RenderPass_Depth(cmdBuffer, 0, GEOM_QUEUE_OPAQUE);
    }
    {
        static constexpr const char* localPassLabelName = "Prev_Frame_Occluders_Depth_Pass_AKill";

        ENG_PROFILE_SCOPED_MARKER_C(localPassLabelName, eng::ProfileColor::Grey51);
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, localPassLabelName, eng::ProfileColor::Grey51);
        RenderPass_Depth(cmdBuffer, 0, GEOM_QUEUE_AKILL);
    }

#ifndef ENG_BUILD_RELEASE
    SetWireframeMode(cmdBuffer, false);
#endif
}


void ThisFrameGeometryDepthPass(vkn::CmdBuffer& cmdBuffer)
{
    if (!s_useDepthPass) {
        return;
    }

    static constexpr const char* passLabelName = "This_Frame_Geom_Depth_Pass";

    ENG_PROFILE_SCOPED_MARKER_C(passLabelName, eng::ProfileColor::Grey51);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, passLabelName, eng::ProfileColor::Grey51);

#ifndef ENG_BUILD_RELEASE
    SetWireframeMode(cmdBuffer, s_geomWireframeMode);
#endif

    {
        static constexpr const char* localPassLabelName = "This_Frame_Geom_Depth_Pass_Opaque";

        ENG_PROFILE_SCOPED_MARKER_C(localPassLabelName, eng::ProfileColor::Grey51);
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, localPassLabelName, eng::ProfileColor::Grey51);
        RenderPass_Depth(cmdBuffer, 1, GEOM_QUEUE_OPAQUE);
    }
    {
        static constexpr const char* localPassLabelName = "This_Frame_Geom_Depth_Pass_AKill";

        ENG_PROFILE_SCOPED_MARKER_C(localPassLabelName, eng::ProfileColor::Grey51);
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, localPassLabelName, eng::ProfileColor::Grey51);
        RenderPass_Depth(cmdBuffer, 1, GEOM_QUEUE_AKILL);
    }

#ifndef ENG_BUILD_RELEASE
    SetWireframeMode(cmdBuffer, false);
#endif
}


static void HZBGeneratePass(vkn::CmdBuffer& cmdBuffer)
{
    if (s_cullingTestMode) {
        return;
    }

    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "HZB_Generation_Pass", eng::ProfileColor::Grey80);
    eng::Timer timer;

    vkn::PSO& pso = s_PSOs[PASS_ID_HZB_GEN];
    
    cmdBuffer.CmdBindPSO(pso);

    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_HZB_GEN, .shaderSetIdx = DESC_SET_PER_DRAW });

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_depthRT, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_DEPTH_BIT)
            .AddTextureBarrier(s_HZB, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1)
        .Push();

    glm::uvec2 srcMipSize = glm::uvec2(s_HZB.GetSizeX(), s_HZB.GetSizeY());
    glm::uvec2 dstMipSize = srcMipSize;

    HZB_GEN_PER_DRAW_DATA pushConsts = {};
    pushConsts.SRC_MIP_RESOLUTION = dstMipSize;
    pushConsts.DST_MIP_RESOLUTION = dstMipSize;
    pushConsts.DST_MIP_IDX = 0;

    cmdBuffer.CmdPushConstants(pso, VK_SHADER_STAGE_COMPUTE_BIT, pushConsts);

    cmdBuffer.CmdDispatch(
        (uint32_t)glm::ceil(s_HZB.GetSizeX() / (float)HZB_BUILD_CS_GROUP_SIZE), 
        (uint32_t)glm::ceil(s_HZB.GetSizeY() / (float)HZB_BUILD_CS_GROUP_SIZE),
        1u
    );

    for (uint32_t mip = 1; mip < s_HZB.GetMipCount(); ++mip) {
        srcMipSize = dstMipSize;
        dstMipSize = glm::max(srcMipSize >> 1u, ONEU2);

        cmdBuffer
            .BeginBarrierList()
                .AddTextureBarrier(s_HZB, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                    VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, 1)
                .AddTextureBarrier(s_HZB, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 
                    VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, mip, 1)
            .Push();

        pushConsts.SRC_MIP_RESOLUTION = srcMipSize;
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


void DepthPass(vkn::CmdBuffer& cmdBuffer)
{
    static constexpr char* markerName = "Depth_Pass";

    ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::Grey80, markerName);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, markerName, eng::ProfileColor::Grey80);

    PrevFrameOccludersCullingPass(cmdBuffer);
    PrevFrameOccludersDepthPass(cmdBuffer);

    HZBGeneratePass(cmdBuffer);

    ThisFrameGeometryCullingPass(cmdBuffer);
    ThisFrameGeometryDepthPass(cmdBuffer);
}


static void CSMGeomCullingClearCounters(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "CSM_Geom_Culling_Clear_Counters", eng::ProfileColor::Black);

    vkn::BarrierList& barriers = cmdBuffer.BeginBarrierList();

    for (uint32_t cascade = 0; cascade < COMMON_CSM_CASCADE_COUNT; ++cascade) {
        for (uint32_t queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            barriers.AddBufferBarrier(s_csmVisGeomIDQueueSizeBuffers[cascade][queue], VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
            barriers.AddBufferBarrier(s_csmGeomBatchQueueSizeBuffers[cascade][queue], VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
            barriers.AddBufferBarrier(s_csmSortedVisGeomIDQueueSizeBuffers[cascade][queue], VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        }
    }

    barriers.Push();

    for (uint32_t cascade = 0; cascade < COMMON_CSM_CASCADE_COUNT; ++cascade) {
        for (uint32_t queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            cmdBuffer.CmdFillBuffer(s_csmVisGeomIDQueueSizeBuffers[cascade][queue], 0, 0, sizeof(glm::uint));
            cmdBuffer.CmdFillBuffer(s_csmGeomBatchQueueSizeBuffers[cascade][queue], 0, 0, sizeof(glm::uint));
            cmdBuffer.CmdFillBuffer(s_csmSortedVisGeomIDQueueSizeBuffers[cascade][queue], 0, 0, sizeof(glm::uint));
        }
    }
}


static void CSMGeomVisIDBufferPass(vkn::CmdBuffer& cmdBuffer)
{
    static constexpr char* markerName = "CSM_Geom_Vis_ID_Buffer_Pass";

    ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::IndianRed1, markerName);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, markerName, eng::ProfileColor::IndianRed1);

    vkn::BarrierList& barriers = cmdBuffer.BeginBarrierList();

    for (uint32_t cascade = 0; cascade < COMMON_CSM_CASCADE_COUNT; ++cascade) {
        for (uint32_t queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            barriers.AddBufferBarrier(s_csmVisGeomIDQueueBuffers[cascade][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
            barriers.AddBufferBarrier(s_csmVisGeomIDQueueSizeBuffers[cascade][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
            barriers.AddBufferBarrier(s_csmGeomBatchDispatchCmdBuffers[cascade][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
        }
    }

    barriers.Push();

    vkn::PSO& pso = s_PSOs[PASS_ID_CSM_GEOM_CULLING];

    cmdBuffer.CmdBindPSO(pso);

    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_CSM_GEOM_CULLING, .shaderSetIdx = DESC_SET_PER_DRAW });

    cmdBuffer.CmdDispatch(ceil(s_cpuInstData.size() / (float)GEOM_CULLING_CS_GROUP_SIZE), 1, 1);
}


static void CSMGeomBatchingPass(vkn::CmdBuffer& cmdBuffer, uint32_t cascade, GEOM_QUEUE queue)
{
    CORE_ASSERT(cascade < COMMON_CSM_CASCADE_COUNT);
    CORE_ASSERT(queue < GEOM_QUEUE_COUNT);

    DescSetID setID;

    switch(queue) {
        case GEOM_QUEUE_OPAQUE:
            setID = (DescSetID)(DESC_SET_ID_CSM_GEOM_BATCHING_OPAQUE_CASCADE_0 + cascade);
            break;
        case GEOM_QUEUE_AKILL:
            setID = (DescSetID)(DESC_SET_ID_CSM_GEOM_BATCHING_AKILL_CASCADE_0 + cascade);
            break;
    }

    cmdBuffer
        .BeginBarrierList()
            .AddBufferBarrier(s_csmVisGeomIDQueueBuffers[cascade][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT)
            .AddBufferBarrier(s_csmVisGeomIDQueueSizeBuffers[cascade][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT)
            .AddBufferBarrier(s_csmGeomBatchQueueBuffers[cascade][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
            .AddBufferBarrier(s_csmGeomBatchQueueSizeBuffers[cascade][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
            .AddBufferBarrier(s_csmSortedVisGeomIDQueueBuffers[cascade][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
            .AddBufferBarrier(s_csmSortedVisGeomIDQueueSizeBuffers[cascade][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
            .AddBufferBarrier(s_csmGeomDrawCmdGenDispatchCmdBuffers[cascade][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)
            
            .AddBufferBarrier(s_csmGeomBatchDispatchCmdBuffers[cascade][queue], VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT)
        .Push();

    vkn::PSO& pso = s_PSOs[PASS_ID_GEOM_BATCHING];

    cmdBuffer.CmdBindPSO(pso);

    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = (uint32_t)setID, .shaderSetIdx = DESC_SET_PER_DRAW });

    cmdBuffer.CmdDispatchIndirect(s_csmGeomBatchDispatchCmdBuffers[cascade][queue]);
}


#define CSM_VIS_GEOM_BATCH_PASS_N_LABEL(CASCADE_IDX) "CSM_Vis_Occluders_Batching_Pass_Cascade_" #CASCADE_IDX

#define CSMVisGeometryBatchingPassCascadeN(CMD_BUFFER, CASCADE_IDX)                                                                \
{                                                                                                                                  \
    ENG_PROFILE_SCOPED_MARKER_C(CSM_VIS_GEOM_BATCH_PASS_N_LABEL(CASCADE_IDX), eng::ProfileColor::LightSteelBlue1);                 \
    ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, CSM_VIS_GEOM_BATCH_PASS_N_LABEL(CASCADE_IDX), eng::ProfileColor::LightSteelBlue1); \
                                                                                                                                   \
    {                                                                                                                              \
        ENG_PROFILE_SCOPED_MARKER_C(CSM_VIS_GEOM_BATCH_PASS_N_LABEL(CASCADE_IDX) "_Opaque", eng::ProfileColor::LightSteelBlue1);                 \
        ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, CSM_VIS_GEOM_BATCH_PASS_N_LABEL(CASCADE_IDX) "_Opaque", eng::ProfileColor::LightSteelBlue1); \
        CSMGeomBatchingPass(CMD_BUFFER, CASCADE_IDX, GEOM_QUEUE_OPAQUE);                                                           \
    }                                                                                                                              \
                                                                                                                                   \
    {                                                                                                                              \
        ENG_PROFILE_SCOPED_MARKER_C(CSM_VIS_GEOM_BATCH_PASS_N_LABEL(CASCADE_IDX) "_AKill", eng::ProfileColor::LightSteelBlue1);                 \
        ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, CSM_VIS_GEOM_BATCH_PASS_N_LABEL(CASCADE_IDX) "_AKill", eng::ProfileColor::LightSteelBlue1); \
        CSMGeomBatchingPass(CMD_BUFFER, CASCADE_IDX, GEOM_QUEUE_AKILL);                                                            \
    }                                                                                                                              \
}


static void CSMVisGeometryBatchingPass(vkn::CmdBuffer& cmdBuffer)
{
    static constexpr char* markerName = "CSM_Vis_Occluders_Batching_Pass";

    ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::LightSteelBlue1, markerName);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, markerName, eng::ProfileColor::LightSteelBlue1);

    CSMVisGeometryBatchingPassCascadeN(cmdBuffer, 0);
    CSMVisGeometryBatchingPassCascadeN(cmdBuffer, 1);
    CSMVisGeometryBatchingPassCascadeN(cmdBuffer, 2);
}


static void CSMGeomDrawCmdGenPass(vkn::CmdBuffer& cmdBuffer, uint32_t cascade, GEOM_QUEUE queue)
{
    CORE_ASSERT(cascade < COMMON_CSM_CASCADE_COUNT);
    CORE_ASSERT(queue < GEOM_QUEUE_COUNT);

    DescSetID setID;

    switch(queue) {
        case GEOM_QUEUE_OPAQUE:
            setID = (DescSetID)(DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_OPAQUE_CASCADE_0 + cascade);
            break;
        case GEOM_QUEUE_AKILL:
            setID = (DescSetID)(DESC_SET_ID_CSM_GEOM_DRAW_CMD_GEN_AKILL_CASCADE_0 + cascade);
            break;
    }

    cmdBuffer
        .BeginBarrierList()
            .AddBufferBarrier(s_csmGeomBatchQueueBuffers[cascade][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT)
            .AddBufferBarrier(s_csmGeomBatchQueueSizeBuffers[cascade][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT)
            .AddBufferBarrier(s_csmGeomDrawCmdQueueBuffers[cascade][queue], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)

            .AddBufferBarrier(s_csmGeomDrawCmdGenDispatchCmdBuffers[cascade][queue], VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT)
        .Push();

    vkn::PSO& pso = s_PSOs[PASS_ID_GEOM_DRAW_CMD_GEN];

    cmdBuffer.CmdBindPSO(pso);

    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
    cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = setID, .shaderSetIdx = DESC_SET_PER_DRAW });

    cmdBuffer.CmdDispatchIndirect(s_csmGeomDrawCmdGenDispatchCmdBuffers[cascade][queue]);
}


#define CSM_VIS_GEOM_DRAW_CMD_GEN_PASS_N_LABEL(CASCADE_IDX) "CSM_Draw_Cmd_Gen_Pass_Cascade_Cascade_" #CASCADE_IDX

#define CSMGeometryDrawCmdGenPassCascadeN(CMD_BUFFER, CASCADE_IDX)                                                                    \
{                                                                                                                                     \
    ENG_PROFILE_SCOPED_MARKER_C(CSM_VIS_GEOM_DRAW_CMD_GEN_PASS_N_LABEL(CASCADE_IDX), eng::ProfileColor::ForestGreen);                 \
    ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, CSM_VIS_GEOM_DRAW_CMD_GEN_PASS_N_LABEL(CASCADE_IDX), eng::ProfileColor::ForestGreen); \
                                                                                                                                      \
    {                                                                                                                                 \
        ENG_PROFILE_SCOPED_MARKER_C(CSM_VIS_GEOM_DRAW_CMD_GEN_PASS_N_LABEL(CASCADE_IDX) "_Opaque", eng::ProfileColor::ForestGreen);                 \
        ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, CSM_VIS_GEOM_DRAW_CMD_GEN_PASS_N_LABEL(CASCADE_IDX) "_Opaque", eng::ProfileColor::ForestGreen); \
        CSMGeomDrawCmdGenPass(CMD_BUFFER, CASCADE_IDX, GEOM_QUEUE_OPAQUE);                                                         \
    }                                                                                                                              \
                                                                                                                                   \
    {                                                                                                                              \
        ENG_PROFILE_SCOPED_MARKER_C(CSM_VIS_GEOM_DRAW_CMD_GEN_PASS_N_LABEL(CASCADE_IDX) "_AKill", eng::ProfileColor::ForestGreen);                 \
        ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, CSM_VIS_GEOM_DRAW_CMD_GEN_PASS_N_LABEL(CASCADE_IDX) "_AKill", eng::ProfileColor::ForestGreen); \
        CSMGeomDrawCmdGenPass(CMD_BUFFER, CASCADE_IDX, GEOM_QUEUE_AKILL);                                                          \
    }                                                                                                                              \
}

static void CSMGeometryDrawCmdGenPass(vkn::CmdBuffer& cmdBuffer)
{
    static constexpr char* markerName = "CSM_Draw_Cmd_Gen_Pass";

    ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::ForestGreen, markerName);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, markerName, eng::ProfileColor::ForestGreen);

    CSMGeometryDrawCmdGenPassCascadeN(cmdBuffer, 0);
    CSMGeometryDrawCmdGenPassCascadeN(cmdBuffer, 1);
    CSMGeometryDrawCmdGenPassCascadeN(cmdBuffer, 2);
}


static void CSMGeometryCullingPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_SCOPED_MARKER_C_FMT(eng::ProfileColor::DodgerBlue1, "CSM_Culling_Pass");
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "CSM_Culling_Pass", eng::ProfileColor::DodgerBlue1);

    CSMGeomCullingClearCounters(cmdBuffer);

    CSMGeomVisIDBufferPass(cmdBuffer);
    CSMVisGeometryBatchingPass(cmdBuffer);
    CSMGeometryDrawCmdGenPass(cmdBuffer);
}


static void RenderPass_CSM(vkn::CmdBuffer& cmdBuffer, uint32_t cascade, GEOM_QUEUE queue)
{
    CORE_ASSERT(cascade < COMMON_CSM_CASCADE_COUNT);
    CORE_ASSERT(queue < GEOM_QUEUE_COUNT);

    const DescSetID setID = DESC_SET_ID_CSM_RENDER;
    
    const bool needClearDepth = queue == GEOM_QUEUE_OPAQUE;

    vkn::Buffer& drawCmdBuffer = s_csmGeomDrawCmdQueueBuffers[cascade][queue];
    vkn::Buffer& drawCmdCountBuffer = s_csmGeomBatchQueueSizeBuffers[cascade][queue];
    vkn::Buffer& visIDBuffer = s_csmSortedVisGeomIDQueueBuffers[cascade][queue];

    cmdBuffer
        .BeginBarrierList()
            .AddBufferBarrier(drawCmdBuffer, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT)
            .AddBufferBarrier(visIDBuffer, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT)
            .AddTextureBarrier(s_csmRT, 
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, 
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 
                VK_IMAGE_ASPECT_DEPTH_BIT, 
                0, VK_REMAINING_MIP_LEVELS, 
                cascade, 1)
        .Push();

    const VkExtent2D extent = VkExtent2D { CSM_CASCADE_RT_SIZE, CSM_CASCADE_RT_SIZE };

    vkn::RenderInfo renderInfo = {};

    renderInfo.renderArea.extent = extent;
    renderInfo.depthAttachment.view = &s_csmRTViews[cascade];
    renderInfo.depthAttachment.loadOp  = needClearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    renderInfo.depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
#ifdef ENG_REVERSED_Z
    renderInfo.depthAttachment.clearValue.depthStencil.depth = 0.f;
#else
    renderInfo.depthAttachment.clearValue.depthStencil.depth = 1.f;
#endif

    cmdBuffer.CmdBeginRendering(renderInfo);
        cmdBuffer.CmdSetViewport(0.f, 0.f, extent.width, extent.height);
        cmdBuffer.CmdSetScissor(0, 0, extent.width, extent.height);

        vkn::PSO& pso = s_PSOs[PASS_ID_CSM_RENDER];

        cmdBuffer.CmdBindPSO(pso);
        
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = (uint32_t)setID, .shaderSetIdx = DESC_SET_PER_DRAW });

        cmdBuffer.CmdBindIndexBuffer(s_geomIndexBuffer, 0, GetVkIndexType());

        CSM_PER_DRAW_DATA pushConsts = {};
        pushConsts.CASCADE_IDX = cascade;
        pushConsts.QUEUE = queue;

        cmdBuffer.CmdPushConstants(pso, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, pushConsts);

        cmdBuffer.CmdDrawIndexedIndirect(drawCmdBuffer, 0, drawCmdCountBuffer, 0, s_cpuInstData.size(), sizeof(COMMON_CMD_DRAW_INDEXED_INDIRECT));
    cmdBuffer.CmdEndRendering();

    cmdBuffer
        .BeginBarrierList()
            .AddTextureBarrier(s_csmRT, 
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, 
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                0, VK_REMAINING_MIP_LEVELS, 
                cascade, 1)
        .Push();
}


#define CSM_RND_PASS_N_LABEL(CASCADE_IDX) "CSM_Render_Pass_Cascade_" #CASCADE_IDX

#define CSMRenderPassCascadeN(CMD_BUFFER, CASCADE_IDX)                                                                        \
{                                                                                                                             \
    ENG_PROFILE_SCOPED_MARKER_C(CSM_RND_PASS_N_LABEL(CASCADE_IDX), eng::ProfileColor::Grey50);                                \
    ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, CSM_RND_PASS_N_LABEL(CASCADE_IDX), eng::ProfileColor::Grey50);                \
                                                                                                                              \
    {                                                                                                                         \
        ENG_PROFILE_SCOPED_MARKER_C(CSM_RND_PASS_N_LABEL(CASCADE_IDX) "_Opaque", eng::ProfileColor::Grey50);                  \
        ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, CSM_RND_PASS_N_LABEL(CASCADE_IDX) "_Opaque", eng::ProfileColor::Grey50);  \
        RenderPass_CSM(CMD_BUFFER, CASCADE_IDX, GEOM_QUEUE_OPAQUE);                                                           \
    }                                                                                                                         \
                                                                                                                              \
    {                                                                                                                         \
        ENG_PROFILE_SCOPED_MARKER_C(CSM_RND_PASS_N_LABEL(CASCADE_IDX) "_AKill", eng::ProfileColor::Grey50);                   \
        ENG_PROFILE_GPU_SCOPED_MARKER_C(CMD_BUFFER, CSM_RND_PASS_N_LABEL(CASCADE_IDX) "_AKill", eng::ProfileColor::Grey50);   \
        RenderPass_CSM(CMD_BUFFER, CASCADE_IDX, GEOM_QUEUE_AKILL);                                                            \
    }                                                                                                                         \
}


static void CSMRenderPass(vkn::CmdBuffer& cmdBuffer)
{
    static constexpr const char* passLabelName = "CSM_Render_Pass";

    ENG_PROFILE_SCOPED_MARKER_C(passLabelName, eng::ProfileColor::Grey50);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, passLabelName, eng::ProfileColor::Grey50);

#ifndef ENG_BUILD_RELEASE
    SetWireframeMode(cmdBuffer, s_geomWireframeMode);
#endif

    CSMRenderPassCascadeN(cmdBuffer, 0);
    CSMRenderPassCascadeN(cmdBuffer, 1);
    CSMRenderPassCascadeN(cmdBuffer, 2);

#ifndef ENG_BUILD_RELEASE
    SetWireframeMode(cmdBuffer, false);
#endif
}


static void CSMPass(vkn::CmdBuffer& cmdBuffer)
{
    static constexpr const char* passLabelName = "CSM_Pass";

    ENG_PROFILE_SCOPED_MARKER_C(passLabelName, eng::ProfileColor::Grey51);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, passLabelName, eng::ProfileColor::Grey51);

    CSMGeometryCullingPass(cmdBuffer);
    CSMRenderPass(cmdBuffer);
}


static void RenderPass_GBuffer(vkn::CmdBuffer& cmdBuffer, uint32_t phase, GEOM_QUEUE queue)
{
    CORE_ASSERT(phase < GEOM_CULLING_PHASES_COUNT);
    CORE_ASSERT(queue < GEOM_QUEUE_COUNT);
    
    DescSetID setID;

    switch(queue) {
        case GEOM_QUEUE_OPAQUE:
            setID = phase == 0 ? DESC_SET_ID_DEPTH_OPAQUE_PHASE_1 : DESC_SET_ID_DEPTH_OPAQUE_PHASE_2;
            break;
        case GEOM_QUEUE_AKILL:
            setID = phase == 0 ? DESC_SET_ID_DEPTH_AKILL_PHASE_1 : DESC_SET_ID_DEPTH_AKILL_PHASE_2;
            break;
    }

    const bool isAKillPass = queue == GEOM_QUEUE_AKILL;

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

    vkn::Buffer& drawCmdBuffer = s_geomDrawCmdQueueBuffers[phase][queue];
    vkn::Buffer& drawCmdCountBuffer = s_geomBatchQueueSizeBuffers[phase][queue];
    vkn::Buffer& visIDBuffer = s_sortedVisGeomIDQueueBuffers[phase][queue];

    barrierList.AddBufferBarrier(drawCmdBuffer, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
    barrierList.AddBufferBarrier(visIDBuffer, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);

    barrierList.Push();    

    const VkExtent2D extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };

    vkn::RenderInfo renderInfo = {};
    renderInfo.renderArea.extent = extent;
    
    renderInfo.depthAttachment.view = &s_depthRTView;
    renderInfo.depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

#ifdef ENG_BUILD_DEBUG
    if (s_useDepthPass) {
        renderInfo.depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    } else {
        renderInfo.depthAttachment.loadOp = isAKillPass ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;

        #ifdef ENG_REVERSED_Z
            renderInfo.depthAttachment.clearValue.depthStencil.depth = 0.f;
        #else
            renderInfo.depthAttachment.clearValue.depthStencil.depth = 1.f;
        #endif
    }
#else
    renderInfo.depthAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
#endif

    std::array<vkn::RenderAttachmentInfo, GBUFFER_RT_COUNT> colorAttachments = {};

    const bool needClearColorAttachments = phase == 0 && queue == GEOM_QUEUE_OPAQUE;

    for (size_t i = 0; i < colorAttachments.size(); ++i) {
        vkn::RenderAttachmentInfo& attachment = colorAttachments[i];

        attachment.view = &s_gbufferRTViews[i];
        attachment.loadOp = needClearColorAttachments ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    }
    
    renderInfo.colorAttachments = colorAttachments;

    cmdBuffer.CmdBeginRendering(renderInfo);
        cmdBuffer.CmdSetViewport(0.f, 0.f, extent.width, extent.height);
        cmdBuffer.CmdSetScissor(0, 0, extent.width, extent.height);

        vkn::PSO& pso = s_PSOs[PASS_ID_GBUFFER];

        cmdBuffer.CmdBindPSO(pso);
        
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = (uint32_t)setID, .shaderSetIdx = DESC_SET_PER_DRAW });

        cmdBuffer.CmdBindIndexBuffer(s_geomIndexBuffer, 0, GetVkIndexType());

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

        cmdBuffer.CmdPushConstants(pso, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, pushConsts);
        
        cmdBuffer.CmdDrawIndexedIndirect(drawCmdBuffer, 0, drawCmdCountBuffer, 0, s_cpuInstData.size(), sizeof(COMMON_CMD_DRAW_INDEXED_INDIRECT));
    cmdBuffer.CmdEndRendering();
}


void GBufferRenderPass(vkn::CmdBuffer& cmdBuffer)
{
    ENG_PROFILE_SCOPED_MARKER_C("GBuffer_Pass", eng::ProfileColor::ForestGreen);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "GBuffer_Pass", eng::ProfileColor::ForestGreen);

#ifndef ENG_BUILD_RELEASE
    SetWireframeMode(cmdBuffer, s_geomWireframeMode);
#endif

    {
        ENG_PROFILE_SCOPED_MARKER_C("GBuffer_Prev_Frame_Occluders_Pass", eng::ProfileColor::ForestGreen);
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "GBuffer_Prev_Frame_Occluders_Pass", eng::ProfileColor::ForestGreen);

        {
            ENG_PROFILE_SCOPED_MARKER_C("GBuffer_Prev_Frame_Occluders_Pass_Opaque", eng::ProfileColor::ForestGreen);
            ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "GBuffer_Prev_Frame_Occluders_Pass_Opaque", eng::ProfileColor::ForestGreen);
            RenderPass_GBuffer(cmdBuffer, 0, GEOM_QUEUE_OPAQUE);
        }

        {
            ENG_PROFILE_SCOPED_MARKER_C("GBuffer_Prev_Frame_Occluders_Pass_AKill", eng::ProfileColor::ForestGreen);
            ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "GBuffer_Prev_Frame_Occluders_Pass_AKill", eng::ProfileColor::ForestGreen);
            RenderPass_GBuffer(cmdBuffer, 0, GEOM_QUEUE_AKILL);
        }
    }

    {
        ENG_PROFILE_SCOPED_MARKER_C("GBuffer_This_Frame_Geometry_Pass", eng::ProfileColor::ForestGreen);
        ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "GBuffer_This_Frame_Geometry_Pass", eng::ProfileColor::ForestGreen);

        {
            ENG_PROFILE_SCOPED_MARKER_C("GBuffer_This_Frame_Geometry_Pass_Opaque", eng::ProfileColor::ForestGreen);
            ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "GBuffer_This_Frame_Geometry_Pass_Opaque", eng::ProfileColor::ForestGreen);
            RenderPass_GBuffer(cmdBuffer, 1, GEOM_QUEUE_OPAQUE);
        }
        
        {
            ENG_PROFILE_SCOPED_MARKER_C("GBuffer_This_Frame_Geometry_Pass_AKill", eng::ProfileColor::ForestGreen);
            ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "GBuffer_This_Frame_Geometry_Pass_AKill", eng::ProfileColor::ForestGreen);
            RenderPass_GBuffer(cmdBuffer, 1, GEOM_QUEUE_AKILL);
        }
    }


#ifndef ENG_BUILD_RELEASE
    SetWireframeMode(cmdBuffer, false);
#endif
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

    vkn::RenderInfo renderInfo = {};
    renderInfo.renderArea.extent = extent;
    
    vkn::RenderAttachmentInfo colorAttachment = {};
    colorAttachment.view = &s_colorRTView16F;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    
    renderInfo.colorAttachments = std::span(&colorAttachment, 1);

    cmdBuffer.CmdBeginRendering(renderInfo);
        cmdBuffer.CmdSetViewport(0.f, 0.f, extent.width, extent.height);
        cmdBuffer.CmdSetScissor(0, 0, extent.width, extent.height);

        vkn::PSO& pso = s_PSOs[PASS_ID_DEFERRED_LIGHTING];

        cmdBuffer.CmdBindPSO(pso);
        
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_DEFERRED_LIGHTING, .shaderSetIdx = DESC_SET_PER_DRAW });

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

    vkn::RenderInfo renderInfo = {};
    renderInfo.renderArea.extent = extent;
    
    vkn::RenderAttachmentInfo colorAttachment = {};
    colorAttachment.view = &s_colorRTView16F,
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    
    renderInfo.colorAttachments = std::span(&colorAttachment, 1);

    renderInfo.depthAttachment.view = &s_depthRTView;
    renderInfo.depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    renderInfo.depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    
    cmdBuffer.CmdBeginRendering(renderInfo);
        cmdBuffer.CmdSetViewport(0.f, 0.f, extent.width, extent.height);
        cmdBuffer.CmdSetScissor(0, 0, extent.width, extent.height);

        vkn::PSO& pso = s_PSOs[PASS_ID_SKYBOX];

        cmdBuffer.CmdBindPSO(pso);
        
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_SKYBOX, .shaderSetIdx = DESC_SET_PER_DRAW });

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

    vkn::RenderInfo renderInfo = {};
    renderInfo.renderArea.extent = extent;
    
    vkn::RenderAttachmentInfo colorAttachment = {};
    colorAttachment.view = &s_colorRTView8U;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    
    renderInfo.colorAttachments = std::span(&colorAttachment, 1);

    cmdBuffer.CmdBeginRendering(renderInfo);
        cmdBuffer.CmdSetViewport(0.f, 0.f, extent.width, extent.height);
        cmdBuffer.CmdSetScissor(0, 0, extent.width, extent.height);

        vkn::PSO& pso = s_PSOs[PASS_ID_POST_PROCESSING];

        cmdBuffer.CmdBindPSO(pso);
        
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_POST_PROCESSING, .shaderSetIdx = DESC_SET_PER_DRAW });

        cmdBuffer.CmdDraw(6, 1, 0, 0);        
    cmdBuffer.CmdEndRendering();
}


#ifdef ENG_DEBUG_DRAW_ENABLED
static void DbgRTViewPass(vkn::CmdBuffer& cmdBuffer)
{
    if (s_dbgOutputRTType == DBG_RT_VIEW_TYPE_NONE) {
        return;
    }

    ENG_PROFILE_SCOPED_MARKER_C("Dbg_RT_View_Render_Pass", eng::ProfileColor::Red);
    ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Dbg_RT_View_Render_Pass", eng::ProfileColor::Red);

    vkn::Texture* pVisTex = nullptr;

    switch (s_dbgOutputRTType) {
        case DBG_RT_VIEW_TYPE_COMMON_DEPTH:
            pVisTex = &s_depthRT;
            break;
        case DBG_RT_VIEW_TYPE_COMMON_HZB:
            pVisTex = &s_HZB;
            break;
        case DBG_RT_VIEW_TYPE_GBUFFER_ALBEDO:
            pVisTex = &s_gbufferRTs[0];
            break;
        case DBG_RT_VIEW_TYPE_GBUFFER_NORMAL:
            pVisTex = &s_gbufferRTs[1];
            break;
        case DBG_RT_VIEW_TYPE_GBUFFER_ROUGHNESS:
            pVisTex = &s_gbufferRTs[2];
            break;
        case DBG_RT_VIEW_TYPE_GBUFFER_METALNESS:
            pVisTex = &s_gbufferRTs[2];
            break;
        case DBG_RT_VIEW_TYPE_GBUFFER_AO:
            pVisTex = &s_gbufferRTs[2];
            break;
        case DBG_RT_VIEW_TYPE_GBUFFER_EMISSIVE:
            pVisTex = &s_gbufferRTs[3];
            break;
        case DBG_RT_VIEW_TYPE_IRRADIANCE_MAP:
            pVisTex = &s_irradianceMapTexture;
            break;
        case DBG_RT_VIEW_TYPE_PREFILTERED_ENV_MAP:
            pVisTex = &s_prefilteredEnvMapTexture;
            break;
        case DBG_RT_VIEW_TYPE_BRDF_LUT:
            pVisTex = &s_brdfLUTTexture;
            break;
        case DBG_RT_VIEW_TYPE_SKYBOX:
            pVisTex = &s_skyboxTexture;
            break;
        case DBG_RT_VIEW_TYPE_CSM_DEPTH:
            pVisTex = &s_csmRT;
            break;
        case DBG_RT_VIEW_TYPE_CSM_CASCADE:
            pVisTex = &s_gbufferRTs[0];
            break;
    }

    VkImageAspectFlagBits aspect;

    if (pVisTex == &s_depthRT || pVisTex == &s_csmRT) {
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else {
        aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    cmdBuffer.BeginBarrierList()
        .AddTextureBarrier(*pVisTex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, 
            VK_ACCESS_2_SHADER_READ_BIT, aspect)
        .AddTextureBarrier(s_colorRT8U, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
    .Push();

    const VkExtent2D extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };

    vkn::RenderInfo renderInfo = {};
    renderInfo.renderArea.extent = extent;
    
    vkn::RenderAttachmentInfo colorAttachment = {};
    colorAttachment.view = &s_colorRTView8U;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    
    renderInfo.colorAttachments = std::span(&colorAttachment, 1);

    cmdBuffer.CmdBeginRendering(renderInfo);
        cmdBuffer.CmdSetViewport(0.f, 0.f, extent.width, extent.height);
        cmdBuffer.CmdSetScissor(0, 0, extent.width, extent.height);

        vkn::PSO& pso = s_PSOs[PASS_ID_DBG_RT_VIEW];

        cmdBuffer.CmdBindPSO(pso);
        
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_DBG_RT_VIEW, .shaderSetIdx = DESC_SET_PER_DRAW });

        DBG_RT_VIEW_PER_DRAW_DATA pushConsts = {};
        pushConsts.MIP = s_dbgOutputRTMip;
        pushConsts.FACE = s_dbgOutputRTFace;
        pushConsts.CSM_CASCADE_IDX = s_dbgOutputRTCascadeIndex;
        pushConsts.DEPTH_Z_NEAR = s_dbgDepthOutputRTZNear;
        pushConsts.DEPTH_Z_FAR = s_dbgDepthOutputRTZFar;

        cmdBuffer.CmdPushConstants(pso, VK_SHADER_STAGE_FRAGMENT_BIT, pushConsts);

        cmdBuffer.CmdDraw(6, 1, 0, 0);
    cmdBuffer.CmdEndRendering();
}


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

    vkn::RenderInfo renderInfo = {};
    renderInfo.renderArea.extent = extent;
    
    vkn::RenderAttachmentInfo colorAttachment = {};
    colorAttachment.view = &s_colorRTView8U;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    
    renderInfo.colorAttachments = std::span(&colorAttachment, 1);

    renderInfo.depthAttachment.view = &s_depthRTView;
    renderInfo.depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    renderInfo.depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    cmdBuffer.CmdBeginRendering(renderInfo);
        cmdBuffer.CmdSetViewport(0.f, 0.f, extent.width, extent.height);
        cmdBuffer.CmdSetScissor(0, 0, extent.width, extent.height);

        if (lineInstCount > 0) {
            ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Dbg_Draw_Render_Pass_Lines", eng::ProfileColor::Red2);
    
            vkn::PSO& pso = s_PSOs[PASS_ID_DBG_DRAW_LINES];

            cmdBuffer.CmdBindPSO(pso);
            
            cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
            cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_DBG_DRAW_LINES, .shaderSetIdx = DESC_SET_PER_DRAW });
    
            cmdBuffer.CmdDraw(DBG_LINE_VERTEX_COUNT, lineInstCount, 0, 0);     
        }

        if (triInstCount > 0) {
            ENG_PROFILE_GPU_SCOPED_MARKER_C(cmdBuffer, "Dbg_Draw_Render_Pass_Triangles", eng::ProfileColor::Red2);

            vkn::PSO& pso = s_PSOs[PASS_ID_DBG_DRAW_TRIANGLES];

            cmdBuffer.CmdBindPSO(pso);
            
            cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
            cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_DBG_DRAW_TRIANGLES, .shaderSetIdx = DESC_SET_PER_DRAW });
    
            cmdBuffer.CmdDraw(DBG_TRIANGLE_VERTEX_COUNT, triInstCount, 0, 0);     
        }
    cmdBuffer.CmdEndRendering();
}
#endif


#ifdef ENG_DEBUG_UI_ENABLED
namespace DbgUI
{
    static void FillData()
    {
        static constexpr ImVec4 IMGUI_RED_COLOR(1.f, 0.f, 0.f, 1.f);
        static constexpr ImVec4 IMGUI_GREEN_COLOR(0.f, 1.f, 0.f, 1.f);

        if (ImGui::Begin("Debug")) {
            if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
                const glm::float3 position = s_mainCamera.GetPosition();
                ImGui::Text("Position: [%.3f, %.3f, %.3f]", position.x, position.y, position.z);

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
                
                ImGui::Text("Fixed CSM Cascades (F7):");
                if (ImGui::IsItemHovered()) {
                    if (ImGui::BeginTooltip()) {
                        ImGui::Text("Show cascades in moment of pressing F7");
                    } ImGui::EndTooltip();
                }
                ImGui::SameLine(); 
                ImGui::TextColored(s_csmTestMode ? IMGUI_GREEN_COLOR : IMGUI_RED_COLOR, s_csmTestMode ? "ON" : "OFF");
            }

            if (ImGui::CollapsingHeader("Memory")) {
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
                for (size_t i = 0; i < COMMON_GEOM_STREAM_COUNT; ++i) {
                    const float kb = s_geomStreamBuffers[i].GetMemorySize() / 1024.f;
                    ImGui::TextDisabled("Geom Stream %s Size: %.3f %s", COMMON_GEOM_STREAM_DBG_NAMES[i], kb > 1024.f ? kb / 1024.f : kb, kb > 1024.f ? "MB": "KB");
                }
                {
                    const float kb = s_geomIndexBuffer.GetMemorySize() / 1024.f;
                    ImGui::TextDisabled("Geom Stream Index Size: %.3f %s", kb > 1024.f ? kb / 1024.f : kb, kb > 1024.f ? "MB": "KB");
                }
                
                ImGui::NewLine();

                {
                    const float kb = s_commonMeshLODBuffer.GetMemorySize() / 1024.f;
                    ImGui::TextDisabled("Geom Mesh LOD Data Size: %.3f %s", kb > 1024.f ? kb / 1024.f : kb, kb > 1024.f ? "MB": "KB");
                }
                {
                    const float kb = s_commonMeshBuffer.GetMemorySize() / 1024.f;
                    ImGui::TextDisabled("Geom Mesh Data Size: %.3f %s", kb > 1024.f ? kb / 1024.f : kb, kb > 1024.f ? "MB": "KB");
                }
                
                ImGui::NewLine();

                {
                    const float kb = s_commonMaterialBuffer.GetMemorySize() / 1024.f;
                    ImGui::TextDisabled("Material Data Size: %.3f %s", kb > 1024.f ? kb / 1024.f : kb, kb > 1024.f ? "MB": "KB");
                }
                
                ImGui::NewLine();

                {
                    const float kb = s_commonInstBuffer.GetMemorySize() / 1024.f;
                    ImGui::TextDisabled("Inst Data Size: %.3f %s", kb > 1024.f ? kb / 1024.f : kb, kb > 1024.f ? "MB": "KB");
                }
                
                ImGui::NewLine();

                ImGui::TextDisabled("Debug Lines Data Size: %.3f KB", (s_dbgLineDataGPU.GetMemorySize() + s_dbgLineVertexDataGPU.GetMemorySize()) / 1024.f);
                ImGui::TextDisabled("Debug Triangles Data Size: %.3f KB", (s_dbgTriangleDataGPU.GetMemorySize() + s_dbgTriangleVertexDataGPU.GetMemorySize()) / 1024.f);
            }
            
            if (ImGui::CollapsingHeader("Geom")) {
                ImGui::Checkbox("Wireframe mode", &s_geomWireframeMode);
                
                if (ImGui::TreeNodeEx("LOD", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::SliderInt("Rorced LOD", &s_forcedGeomLOD, -1, MAX_GEOM_LOD_COUNT);

                    if (ImGui::IsItemHovered()) {
                        if (ImGui::BeginTooltip()) {
                            ImGui::Text("Forced LOD -1 means \'Do not force any LOD\'");
                        } ImGui::EndTooltip();
                    }

                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Culling", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("##GeomCulling", &s_useMeshCulling);
                    ImGui::SameLine(); 
                    ImGui::TextColored(s_useMeshCulling ? IMGUI_GREEN_COLOR : IMGUI_RED_COLOR, "Enabled");
        
                    if (s_useMeshCulling) {
                        if (ImGui::TreeNodeEx("Types", ImGuiTreeNodeFlags_DefaultOpen)) {
                            if (ImGui::TreeNodeEx("Frustum", ImGuiTreeNodeFlags_DefaultOpen)) {
                                ImGui::Checkbox("##FrustumCulling", &s_useMeshFrustumCulling);
                                ImGui::SameLine(); 
                                ImGui::TextColored(s_useMeshFrustumCulling ? IMGUI_GREEN_COLOR : IMGUI_RED_COLOR, "Enabled");
    
                                ImGui::TreePop();
                            }
                            
                            if (ImGui::TreeNodeEx("HZB", ImGuiTreeNodeFlags_DefaultOpen)) {
                                ImGui::Checkbox("##HZBCulling", &s_useMeshHZBCulling);
                                ImGui::SameLine(); 
                                ImGui::TextColored(s_useMeshHZBCulling ? IMGUI_GREEN_COLOR : IMGUI_RED_COLOR, "Enabled");
    
                                ImGui::TreePop();
                            }
                        
                            ImGui::TreePop();
                        }
                    }

                    ImGui::TreePop();
                }
            }
            
            if (ImGui::CollapsingHeader("Passes")) {
                if (ImGui::TreeNodeEx("Depth", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("##DepthPassEnabled", &s_useDepthPass);
                    ImGui::SameLine();
                    ImGui::TextColored(s_useDepthPass ? IMGUI_GREEN_COLOR : IMGUI_RED_COLOR, "Enabled");

                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Deferred Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("##UseIndirectLighting", &s_useIndirectLighting);
                    ImGui::SameLine(); 
                    ImGui::TextColored(s_useIndirectLighting ? IMGUI_GREEN_COLOR : IMGUI_RED_COLOR, "Use Indirect Lighting");

                    ImGui::TreePop();
                }
            }

            if (ImGui::CollapsingHeader("Tonemapping")) {
                if (ImGui::BeginCombo("Preset", DBG_TONEMAPPING_NAMES[(size_t)s_tonemappingPreset])) {
                    for (size_t i = 0; i < _countof(DBG_TONEMAPPING_NAMES); ++i) {
                        const bool isSelected = (DBG_TONEMAPPING_NAMES[i] == DBG_TONEMAPPING_NAMES[(size_t)s_tonemappingPreset]);
                        
                        if (ImGui::Selectable(DBG_TONEMAPPING_NAMES[i], isSelected)) {
                            s_tonemappingPreset = TonemapPreset(i);
                        }
                        
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            }

        #ifdef ENG_BUILD_DEBUG
            if (ImGui::CollapsingHeader("Debug Vis")) {
                ImGui::Checkbox("##DrawInstanceAABB", &s_drawInstAABBs);
                ImGui::SameLine();
                ImGui::TextColored(s_drawInstAABBs ? IMGUI_GREEN_COLOR : IMGUI_RED_COLOR, "Draw Instance AABB");

                if (ImGui::BeginCombo("Render Target", DBG_RT_OUTPUT_NAMES[s_dbgOutputRTType])) {
                    for (size_t i = 0; i < _countof(DBG_RT_OUTPUT_NAMES); ++i) {
                        const bool isSelected = (DBG_RT_OUTPUT_NAMES[i] == DBG_RT_OUTPUT_NAMES[s_dbgOutputRTType]);
                        
                        if (ImGui::Selectable(DBG_RT_OUTPUT_NAMES[i], isSelected)) {
                            s_dbgOutputRTType = DBG_RT_VIEW_TYPE(i);
                        }
                        
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                bool needExtraZNearFar = false;
                bool needExtraMip = false;
                bool needExtraFace = false;
                bool needExtraCascadeIndex = false;

                switch (s_dbgOutputRTType) {
                    case DBG_RT_VIEW_TYPE_NONE:
                        break;
                    case DBG_RT_VIEW_TYPE_COMMON_DEPTH:
                        needExtraZNearFar = true;
                        break;
                    case DBG_RT_VIEW_TYPE_COMMON_HZB:
                        needExtraZNearFar = true;
                        needExtraMip = true;
                        break;
                    case DBG_RT_VIEW_TYPE_GBUFFER_ALBEDO:
                        break;
                    case DBG_RT_VIEW_TYPE_GBUFFER_NORMAL:
                        break;
                    case DBG_RT_VIEW_TYPE_GBUFFER_ROUGHNESS:
                        break;
                    case DBG_RT_VIEW_TYPE_GBUFFER_METALNESS:
                        break;
                    case DBG_RT_VIEW_TYPE_GBUFFER_AO:
                        break;
                    case DBG_RT_VIEW_TYPE_GBUFFER_EMISSIVE:
                        break;
                    case DBG_RT_VIEW_TYPE_IRRADIANCE_MAP:
                        needExtraFace = true;
                        break;
                    case DBG_RT_VIEW_TYPE_PREFILTERED_ENV_MAP:
                        needExtraMip = true;
                        needExtraFace = true;
                        break;
                    case DBG_RT_VIEW_TYPE_BRDF_LUT:
                        break;
                    case DBG_RT_VIEW_TYPE_SKYBOX:
                        needExtraFace = true;
                        needExtraMip = true;
                        break;
                    case DBG_RT_VIEW_TYPE_CSM_DEPTH:
                        needExtraCascadeIndex = true;
                        break;
                }

                const bool needExtraParams = needExtraZNearFar || needExtraMip || needExtraFace || needExtraCascadeIndex;

                if (needExtraParams && ImGui::TreeNodeEx("RT Vis Params", ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (needExtraZNearFar) {
                        ImGui::SliderFloat("Z Near", &s_dbgDepthOutputRTZNear, s_mainCamera.GetZNear(), s_mainCamera.GetZFar(), "%.2f");
                        ImGui::SliderFloat("Z Far", &s_dbgDepthOutputRTZFar, s_mainCamera.GetZNear(), s_mainCamera.GetZFar(), "%.2f");
                        
                        if (s_dbgDepthOutputRTZFar <= s_dbgDepthOutputRTZNear) {
                            s_dbgDepthOutputRTZFar = s_dbgDepthOutputRTZNear + 0.1f;
                        }
                    }

                    if (needExtraMip) {
                        ImGui::SliderInt("Mip", &s_dbgOutputRTMip, 0, glm::min((int32_t)HZB_MAX_MIP_COUNT - 1, 20));
                    }

                    if (needExtraFace) {
                        static constexpr const char* FACE_NAMES[M3D_CUBEMAP_FACE_COUNT] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };

                        if (ImGui::BeginCombo("Face", FACE_NAMES[s_dbgOutputRTFace])) {
                            for (size_t i = 0; i < M3D_CUBEMAP_FACE_COUNT; ++i) {
                                const bool isSelected = (FACE_NAMES[i] == FACE_NAMES[s_dbgOutputRTFace]);
                                
                                if (ImGui::Selectable(FACE_NAMES[i], isSelected)) {
                                    s_dbgOutputRTFace = i;
                                }
                                
                                if (isSelected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }

                    if (needExtraCascadeIndex) {
                        ImGui::SliderInt("Cascade", &s_dbgOutputRTCascadeIndex, 0, (int32_t)COMMON_CSM_CASCADE_COUNT - 1);
                    }

                    ImGui::TreePop();
                }
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

    vkn::RenderInfo renderInfo = {};
    renderInfo.renderArea.extent = VkExtent2D { s_pWnd->GetWidth(), s_pWnd->GetHeight() };
    
    vkn::RenderAttachmentInfo colorAttachment = {};
    colorAttachment.view = &s_colorRTView8U;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    
    renderInfo.colorAttachments = std::span(&colorAttachment, 1);

    cmdBuffer.CmdBeginRendering(renderInfo);
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

    vkn::RenderInfo renderInfo = {};
    renderInfo.renderArea.extent = extent;
    
    vkn::RenderAttachmentInfo colorAttachment = {};
    colorAttachment.view = &scTextureView;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    
    renderInfo.colorAttachments = std::span(&colorAttachment, 1);

    cmdBuffer.CmdBeginRendering(renderInfo);
        cmdBuffer.CmdSetViewport(0.f, 0.f, extent.width, extent.height);
        cmdBuffer.CmdSetScissor(0, 0, extent.width, extent.height);

        vkn::PSO& pso = s_PSOs[PASS_ID_BACKBUFFER];

        cmdBuffer.CmdBindPSO(pso);
        
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_COMMON, .shaderSetIdx = DESC_SET_PER_FRAME });
        cmdBuffer.CmdBindDescriptorBufferSets(pso, { .elemIndex = DESC_SET_ID_BACKBUFFER, .shaderSetIdx = DESC_SET_PER_DRAW });

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
    UpdateGPUDbgConstBuffer();

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

        DepthPass(cmdBuffer);

        CSMPass(cmdBuffer);

        GBufferRenderPass(cmdBuffer);
        DeferredLightingPass(cmdBuffer);

        SkyboxPass(cmdBuffer);

        PostProcessingPass(cmdBuffer);

    #ifdef ENG_DEBUG_DRAW_ENABLED
        DbgRTViewPass(cmdBuffer);
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
                s_mainCameraVel.z = -finalSpeed;
            }
            if (keyEvent.key == eng::WndKey::KEY_S) {
                s_mainCameraVel.z = finalSpeed;
            }
            if (keyEvent.key == eng::WndKey::KEY_A) {
                s_mainCameraVel.x = -finalSpeed;
            }
            if (keyEvent.key == eng::WndKey::KEY_D) {
                s_mainCameraVel.x = finalSpeed;
            }
            if (keyEvent.key == eng::WndKey::KEY_E) {
                s_mainCameraVel.y = finalSpeed;
            }
            if (keyEvent.key == eng::WndKey::KEY_Q) {
                s_mainCameraVel.y = -finalSpeed;
            }
            if (keyEvent.key == eng::WndKey::KEY_F5) {
                firstEvent = true;
            }
        }

        if (keyEvent.IsReleased()) {
            if (keyEvent.key == eng::WndKey::KEY_W) {
                s_mainCameraVel.z = 0;
            }
            if (keyEvent.key == eng::WndKey::KEY_S) {
                s_mainCameraVel.z = 0;
            }
            if (keyEvent.key == eng::WndKey::KEY_A) {
                s_mainCameraVel.x = 0;
            }
            if (keyEvent.key == eng::WndKey::KEY_D) {
                s_mainCameraVel.x = 0;
            }
            if (keyEvent.key == eng::WndKey::KEY_E) {
                s_mainCameraVel.y = 0;
            }
            if (keyEvent.key == eng::WndKey::KEY_Q) {
                s_mainCameraVel.y = 0;
            }
        }
    } else if (event.Is<eng::WndCursorEvent>()) {
        CORE_ASSERT(s_pWnd->IsCursorRelativeMode());

        static glm::float3 pitchYawRoll = s_mainCamera.GetPitchYawRollDegrees();

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
                s_fixedCamCullViewProjMatr = s_mainCamera.GetViewProjMatrix();
                s_fixedCamCullInvViewProjMatr = s_mainCamera.GetInvViewProjMatrix();
                s_fixedCamCullFrustum = s_mainCamera.GetFrustum();
            }
        } else if (keyEvent.key == eng::WndKey::KEY_F7 && keyEvent.IsPressed()) {
            s_csmTestMode = !s_csmTestMode;

            if (s_csmTestMode) {
                for (size_t i = 0; i < COMMON_CSM_CASCADE_COUNT; ++i) {
                    s_fixedCamCsmInvViewProjMatr[i] = s_csmCameras[i].GetInvViewProjMatrix();
                }
            }
        }
    }

    if (s_flyCameraMode) {
        CameraProcessWndEvent(s_mainCamera, event);
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
    InitWindow();

    // LoadScene(argc > 1 ? argv[1] : "../assets/Sponza/Sponza.gltf");
    // LoadScene(argc > 1 ? argv[1] : "../assets/LightSponza/Sponza.gltf");
    // LoadScene(argc > 1 ? argv[1] : "../assets/TestPBR/TestPBR.gltf");
    // LoadScene(argc > 1 ? argv[1] : "../assets/GPUOcclusionTest/Occlusion.gltf");
    LoadScene(argc > 1 ? argv[1] : "../assets/ShadowTest/ShadowTest.gltf");

    CreateVkInstance();    
    CreateVkSurface();    
    CreateVkPhysAndLogicalDevices();

#ifdef ENG_PROFILING_ENABLED
    vkn::GetProfiler().Create(&s_vkDevice);
    CORE_ASSERT(vkn::GetProfiler().IsCreated());
#endif

    CreateVkSwapchain();
    CreateVkMemoryAllocator();

    CreateCommonCmdPool();

    CreateImmediateSubmitObjects();

    CreateCommonStagingBuffer();

    CreateDynamicRenderTargets();

    CreateCommonSamplers();
    CreateCommonConstBuffer();
    CreateCommonDbgConstBuffer();
    CreateGeomCullingAndInstancingResources();
    CreateCSMGeomCullingAndInstancingResources();
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

    CreateSyncObjects();
    
    s_pRenderCmdBuffer = s_commonCmdPool.AllocCmdBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    s_vkDevice.SetObjDebugName(*s_pRenderCmdBuffer, "RND_CMD_BUFFER");

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

    s_pWnd->SetVisible(true);

    while(!s_pWnd->IsClosed()) {
        ProcessFrame();
    }

    s_vkDevice.WaitIdle();

    return 0;
}