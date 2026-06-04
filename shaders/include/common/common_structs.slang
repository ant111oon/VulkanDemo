#ifndef COMMON_STRUCTS_H
#define COMMON_STRUCTS_H

#include "common/math/geom_primitives.slang"
#include "common/math/constants.slang"


static const uint  COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT = 10;
static const float COMMON_PREFILTERED_ENV_MAP_MIP_ROUGHNESS_DELTA = 1.f / (COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1);

static const uint2 COMMON_IRRADIANCE_MAP_SIZE = UINT2(32);
static const uint2 COMMON_PREFILTERED_ENV_MAP_SIZE = UINT2(uint(1) << (COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1));
static const uint2 COMMON_BRDF_INTEGRATION_LUT_SIZE = uint2(512);


enum COMMON_GEOM_STREAM_ID
{
    POSITION,
    NORMAL,
    UV,
    TANGENT,
    COUNT
};


enum COMMON_DBG_TEX_IDX
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


enum COMMON_MATERIAL_FLAGS
{
    DOUBLE_SIDED = 0x1,
    ALPHA_KILL = 0x2,
};


struct COMMON_MATERIAL
{
    bool IsAlphaKill() { return IS_FLAG_SET(FLAGS, COMMON_MATERIAL_FLAGS::ALPHA_KILL); }
    bool IsOpaque() { return !IsAlphaKill(); }

    bool IsDoubleSided() { return IS_FLAG_SET(FLAGS, COMMON_MATERIAL_FLAGS::DOUBLE_SIDED); }

    float4 ALBEDO_MULT;

    float3 EMISSIVE_MULT;
    float ALPHA_REF;

    float NORMAL_SCALE;
    float METALNESS_SCALE;
    float ROUGHNESS_SCALE;
    float AO_COEF;

    int ALBEDO_TEX_IDX;
    int NORMAL_TEX_IDX;
    int MR_TEX_IDX;
    int AO_TEX_IDX;

    uint2 PAD0;
    int EMISSIVE_TEX_IDX;
    uint FLAGS;
};


struct COMMON_MESH_LOD
{
    uint FIRST_INDEX;
    uint INDEX_COUNT;
};


struct COMMON_MESH
{
    AABB GetBounds()
    {
        return AABB(
            float3(unpackHalf2x16ToFloat(BOUNDS_MIN_MAX_LCS_PACKED.x), unpackHalf2x16ToFloat(BOUNDS_MIN_MAX_LCS_PACKED.y).x),
            float3(unpackHalf2x16ToFloat(BOUNDS_MIN_MAX_LCS_PACKED.y).y, unpackHalf2x16ToFloat(BOUNDS_MIN_MAX_LCS_PACKED.z))
        );
    }

    uint FIRST_VERTEX;
    uint VERTEX_COUNT;

    uint FIRST_LOD; // Index of mesh LOD 0 inside LOD buffer
    uint LOD_COUNT;

    uint3 BOUNDS_MIN_MAX_LCS_PACKED; // x - MIN.xy, y - MIN.z and MAX.x, z - MAX.yz
    uint  PADD;
};


struct COMMON_INST_AABB
{
    AABB GetAABB()
    {
        const float3 minn = float3(unpackHalf2x16ToFloat(MIN_MAX_PACKED.x), unpackHalf2x16ToFloat(MIN_MAX_PACKED.y).x);
        const float3 maxx = float3(unpackHalf2x16ToFloat(MIN_MAX_PACKED.y).y, unpackHalf2x16ToFloat(MIN_MAX_PACKED.z));

        return AABB(minn, maxx);
    }

    uint3 MIN_MAX_PACKED; // x - MIN.xy, y - MIN.z and MAX.x, z - MAX.yz
    uint PADDING;
};


struct COMMON_INST
{
    uint TRANSFORM_IDX;
    uint MATERIAL_IDX;
    uint MESH_IDX;
    uint VOLUME_IDX;
};


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

    int  DBG_FORCED_GEOM_LOD;
    uint DBG_FLAGS;
    uint DBG_VIS_FLAGS;
    uint PAD0;
};


enum class COMMON_DBG_FLAG_MASKS
{
    USE_REINHARD_TONE_MAPPING_MASK = 0x1,
    USE_PARTIAL_UNCHARTED_2_TONE_MAPPING_MASK = 0x2,
    USE_UNCHARTED_2_TONE_MAPPING_MASK = 0x4,
    USE_ACES_TONE_MAPPING_MASK = 0x8,
    USE_INDIRECT_LIGHTING_MASK = 0x10,
    USE_GEOM_GPU_FRUSTUM_CULLING_MASK = 0x20,
    USE_GEOM_GPU_HZB_CULLING_MASK = 0x40,
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


enum COMMON_SAMPLER_IDX
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

    COUNT
};


struct COMMON_CMD_DRAW_INDEXED_INDIRECT
{
    // NOTE: Don't change order of this variables!!!
    uint INDEX_COUNT;
    uint INSTANCE_COUNT;
    uint FIRST_INDEX;
    int  FIRST_VERTEX;
    uint FIRST_INSTANCE;
};


#endif