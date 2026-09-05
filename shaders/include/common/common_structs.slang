#ifndef COMMON_STRUCTS_H
#define COMMON_STRUCTS_H

#include "common/math/geom_primitives.slang"
#include "common/math/bits.slang"


static const uint COMMON_MAX_GEOM_LOD_COUNT = 6;

static const uint  COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT = 10;
static const float COMMON_PREFILTERED_ENV_MAP_MIP_ROUGHNESS_DELTA = 1.f / (COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1);

static const uint2 COMMON_IRRADIANCE_MAP_SIZE = UINT2(32);
static const uint2 COMMON_PREFILTERED_ENV_MAP_SIZE = UINT2(uint(1) << (COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1));
static const uint2 COMMON_BRDF_INTEGRATION_LUT_SIZE = uint2(512);

static const uint COMMON_CSM_CASCADE_COUNT = 3;
static const uint COMMON_CSM_CASCADE_BOUNDS_COUNT = COMMON_CSM_CASCADE_COUNT + 1;

static const uint COMMON_HZB_MAX_MIP_COUNT = 12; // Corresponds to 4K resolution


enum GPU_DbgRTViewType
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
    CSM_HZB,

    COUNT,
};


enum GPU_DbgTonemapPreset
{
    ACES,
    REINHARD,
    PARTIAL_UNCHARTED_2,
    UNCHARTED_2,

    COUNT
};


enum GPU_DbgCsmPCFPreset
{
    NONE,
    GRID,
    POISSON_DISK,
    VOGEL_DISK,

    COUNT
};


enum GPU_CommonSamplerID
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


enum GPU_CommonCmpSamplerID
{
    LINEAR_CLAMP_TO_BORDER_LESS,
    LINEAR_CLAMP_TO_BORDER_LESS_OR_EQUAL,

    LINEAR_CLAMP_TO_BORDER_GREATER,
    LINEAR_CLAMP_TO_BORDER_GREATER_OR_EQUAL,

    COUNT
};


struct GPU_CommonCSMPcssData
{
    float lightAngularSlope;
    float maxSearchRadiusTexels;
    float maxFilterRadiusTexels;
    float minFilterRadiusTexels;

    float searchRadiusScale;
    float filterRadiusScale;
    uint  searchSamplesCount;
    uint  filterSamplesCount;
};


struct GPU_CommonCSMData
{
    GPU_Frustum viewFrustums[COMMON_CSM_CASCADE_COUNT];
    float4x4 viewMatrices[COMMON_CSM_CASCADE_COUNT];
    float4x4 viewProjMatrices[COMMON_CSM_CASCADE_COUNT];
    
    float4 cascadeDistances;
    float4 cascadeZNear;
    float4 cascadeZFar;
    float4 cascadeWorldUnitsPerPixel;

    float cascadeBlendThresholdCoef;
    uint  filterDiskSampleCount;
    float filterDiskRadius;
    uint  filterGridHalfSize;

    uint2 texSize;
    uint2 padding;

    GPU_CommonCSMPcssData pcssData;
};


struct GPU_CommonCBData
{
    GPU_Frustum mainCamFrustum;
    GPU_Frustum cullingFrustum; // In most cases is the same as mainCamFrustum but can differ if culling debug mode is enabled

    float4x4 mainCamViewMatr;
    float4x4 mainCamProjMatr;
    float4x4 mainCamViewProjMatr;

    float4x4 mainCamInvViewMatr;
    float4x4 mainCamInvProjMatr;
    float4x4 mainCamInvViewProjMatr;

    float4x4 cullingViewProjMatrPrev;
    float4x4 cullingViewProjMatr; // In most cases is the same as mainCamViewProjMatr but can differ if culling debug mode is enabled

    uint2 screenSize;
    float mainCamZNear;
    float mainCamZFar;

    float3 mainCamWPos;
    uint flags;

    float3 sunLightDir;
    uint sunLightColor;

    GPU_CommonCSMData csmData;
};


struct GPU_CommonDbgCBData
{
    int  forcedGeomLOD;
    uint flags_0;

    GPU_DbgRTViewType rtViewType;
    GPU_DbgTonemapPreset tonemapPreset;

    private uint3 padding_0;
    GPU_DbgCsmPCFPreset csmPCFPreset;
};

#endif