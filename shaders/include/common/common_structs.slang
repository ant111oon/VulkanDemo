#ifndef COMMON_STRUCTS_H
#define COMMON_STRUCTS_H

#include "common/math/geom_primitives.slang"


static const uint  COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT = 10;
static const float COMMON_PREFILTERED_ENV_MAP_MIP_ROUGHNESS_DELTA = 1.f / (COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1);

static const uint2 COMMON_IRRADIANCE_MAP_SIZE = UINT2(32);
static const uint2 COMMON_PREFILTERED_ENV_MAP_SIZE = UINT2(uint(1) << (COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1));
static const uint2 COMMON_BRDF_INTEGRATION_LUT_SIZE = uint2(512);

static const uint COMMON_CSM_CASCADE_COUNT = 3;
static const uint COMMON_CSM_CASCADE_BOUNDS_COUNT = COMMON_CSM_CASCADE_COUNT + 1;


enum DBG_TEX_IDX
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


enum DBG_RT_VIEW_TYPE
{
    NONE,
    COMMON_DEPTH,
    COMMON_HZB,
    GBUFFER_ALBEDO,
    GBUFFER_NORMAL,
    GBUFFER_ROUGHNESS,
    GBUFFER_METALNESS,
    GBUFFER_AO,
    GBUFFER_EMISSIVE,
    IRRADIANCE_MAP,
    PREFILTERED_ENV_MAP,
    BRDF_LUT,
    SKYBOX,
    CSM_DEPTH,

    COUNT,
};


enum DBG_TONEMAP_PRESET
{
    ACES,
    REINHARD,
    PARTIAL_UNCHARTED_2,
    UNCHARTED_2,

    COUNT
};


enum DBG_CSM_PCF_PRESET
{
    NONE,
    PCF_2X2,
    PCF_3X3,

    COUNT
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


enum COMMON_CMP_SAMPLER_IDX
{
    LINEAR_CLAMP_TO_BORDER_LESS,
    LINEAR_CLAMP_TO_BORDER_LESS_OR_EQUAL,

    LINEAR_CLAMP_TO_BORDER_GREATER,
    LINEAR_CLAMP_TO_BORDER_GREATER_OR_EQUAL,

    COUNT
};


enum COMMON_GEOM_STREAM_ID
{
    POSITION,
    NORMAL,
    UV,
    TANGENT,
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

    private uint2 PADDING;
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
    property AABB AABB_LCS
    {
        get
        {
            const float3 minn = float3(unpackHalf2x16ToFloat(AABB_MIN_MAX_LCS_PACKED.x), unpackHalf2x16ToFloat(AABB_MIN_MAX_LCS_PACKED.y).x);
            const float3 maxx = float3(unpackHalf2x16ToFloat(AABB_MIN_MAX_LCS_PACKED.y).y, unpackHalf2x16ToFloat(AABB_MIN_MAX_LCS_PACKED.z));

            return AABB(minn, maxx);
        }
    }

    uint FIRST_VERTEX;
    uint VERTEX_COUNT;

    uint FIRST_LOD; // Index of mesh LOD 0 inside LOD buffer
    uint LOD_COUNT;

    private uint3 AABB_MIN_MAX_LCS_PACKED; // x - MIN.xy, y - MIN.z and MAX.x, z - MAX.yz
    private uint  PADDING;
};


struct COMMON_INST
{
    property AABB AABB_WCS
    {
        get
        {
            const float3 minn = float3(unpackHalf2x16ToFloat(AABB_MIN_MAX_WCS_PACKED.x), unpackHalf2x16ToFloat(AABB_MIN_MAX_WCS_PACKED.y).x);
            const float3 maxx = float3(unpackHalf2x16ToFloat(AABB_MIN_MAX_WCS_PACKED.y).y, unpackHalf2x16ToFloat(AABB_MIN_MAX_WCS_PACKED.z));

            return AABB(minn, maxx);
        }
    }

    row_major float3x4 MATR_WCS;

    private uint3 AABB_MIN_MAX_WCS_PACKED; // x - MIN.xy, y - MIN.z and MAX.x, z - MAX.yz
    private uint  PADDING_0;

    uint MESH_IDX;
    uint MATERIAL_IDX;
    private uint2 PADDING_1;
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

    float3 SUN_LIGHT_DIRECTION;
    uint SUN_LIGHT_COLOR;

    FRUSTUM CSM_VIEW_FRUSTUMS[COMMON_CSM_CASCADE_COUNT];
    float4x4 CSM_VIEW_MATRICES[COMMON_CSM_CASCADE_COUNT];
    float4x4 CSM_VIEW_PROJ_MATRICES[COMMON_CSM_CASCADE_COUNT];
    float4 CSM_CASCADE_DISTANCES;

    uint3 PADDING_0;
    float CSM_CASCADE_BLEND_THRESHOLD_COEF;
};


struct COMMON_DBG_CB_DATA
{
    int  FORCED_GEOM_LOD;
    uint FLAGS_0;

    DBG_RT_VIEW_TYPE RT_VIEW_TYPE;
    DBG_TONEMAP_PRESET TONEMAP_PRESET;

    uint3 PADDING_0;
    DBG_CSM_PCF_PRESET CSM_PCF_PRESET;
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


struct COMMON_CMD_DISPATCH_INDIRECT
{
    // NOTE: Don't change order of this variables!!!
    uint GROUP_SIZE_X;
    uint GROUP_SIZE_Y;
    uint GROUP_SIZE_Z;
};


#endif