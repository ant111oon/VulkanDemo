#ifndef COMMON_STRUCTS_H
#define COMMON_STRUCTS_H

#include "common/math/geom_primitives.slang"
#include "common/math/bits.slang"


static const uint  COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT = 10;
static const float COMMON_PREFILTERED_ENV_MAP_MIP_ROUGHNESS_DELTA = 1.f / (COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1);

static const uint2 COMMON_IRRADIANCE_MAP_SIZE = UINT2(32);
static const uint2 COMMON_PREFILTERED_ENV_MAP_SIZE = UINT2(uint(1) << (COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1));
static const uint2 COMMON_BRDF_INTEGRATION_LUT_SIZE = uint2(512);

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


enum GPU_SamplerID
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


enum GPU_CmpSamplerID
{
    LINEAR_CLAMP_TO_BORDER_LESS,
    LINEAR_CLAMP_TO_BORDER_LESS_OR_EQUAL,

    LINEAR_CLAMP_TO_BORDER_GREATER,
    LINEAR_CLAMP_TO_BORDER_GREATER_OR_EQUAL,

    COUNT
};


struct GPU_CommonCameraData
{
    GPU_Frustum frustum;

    float4x4 viewMatr;
    float4x4 projMatr;
    float4x4 viewProjMatr;
    float4x4 viewProjMatrPrev;

    float4x4 invViewMatr;
    float4x4 invProjMatr;
    float4x4 invViewProjMatr;
    float4x4 invViewProjMatrPrev;

    float3 wPos;
    float zNear;
    
    float3 padding;
    float zFar;
};


struct GPU_CommonCBData
{
    GPU_CommonCameraData mainCam;

    uint2 screenSize;
    uint flags;
    uint padding;
};


struct GPU_CommonDbgCBData
{
    property bool isIBLEnabled
    {
        get { return (bool)enableIBL; }
    }

    property bool isCsmEnabled
    {
        get { return (bool)enableCSM; }
    }

    property bool isCsmVisualizatioEnabled
    {
        get { return (bool)enableCsmVisualization; }
    }

    property bool isCsmSplitBlendEnabled
    {
        get { return (bool)enableCsmSplitBlend; }
    }

    property bool isCsmFilterRandOffsetsEnabled
    {
        get { return (bool)enableCsmFilterRandOffsets; }
    }

    property bool isCsmPCSSEnabled
    {
        get { return (bool)enableCsmPCSS; }
    }

    property bool isCsmPCSSRandRotationEnabled
    {
        get { return (bool)enableCsmPCSSRandRotation; }
    }

    property bool isGeomLODVisualizationEnabled
    {
        get { return (bool)enableGeomLODVisualization; }
    }

    GPU_DbgRTViewType rtViewType;
    GPU_DbgTonemapPreset tonemapPreset;
    GPU_DbgCsmPCFPreset csmPCFPreset;
    int forcedGeomLOD;
    
    private uint3 padding;
    private uint enableIBL : 1;
    private uint enableCSM : 1;
    private uint enableCsmVisualization : 1;
    private uint enableCsmSplitBlend : 1;
    private uint enableCsmFilterRandOffsets : 1;
    private uint enableCsmPCSS : 1;
    private uint enableCsmPCSSRandRotation : 1;
    private uint enableGeomLODVisualization : 1;
    private uint padding_1 : 24;
};

#endif