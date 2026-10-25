#ifndef COMMON_LIGHTING_STRUCTS_H
#define COMMON_LIGHTING_STRUCTS_H

#include "common/math/geom_primitives.slang"
#include "brdf.slang"

#include "common_structs.slang"


static const uint CSM_SPLIT_COUNT = 3;
static const uint CSM_SPLIT_BOUNDS_COUNT = CSM_SPLIT_COUNT + 1;

static const float CSM_MIN_BIAS = 0.0001f;
static const float CSM_MAX_BIAS = 0.01f;
static const float CSM_CONSTANT_BIAS = 0.0001f;
static const float CSM_SLOPE_BIAS = 0.0005f;

static const uint CSM_PCSS_SEARCH_ROTATION_SALT = 0x68bc21ebu;
static const uint CSM_PCSS_FILTER_ROTATION_SALT = 0x02e5be93u;

static const float3 DBG_CSM_SPLIT_COLORS[] = {
    float3(10.f, 0.f, 0.f),
    float3(0.f, 10.f, 0.f),
    float3(0.f, 0.f, 10.f),
    float3(0.004f, 0.004f, 0.004f),
};


struct GPU_CsmSplitData
{
    GPU_CommonCameraData camData;

    float2 zBounds;
    uint2  rtSize;

    float2 worldUnitsPerPixel;
    private uint padding;
};


enum GPU_PCFType
{
    NONE,
    GRID,
    POISSON_DISK,
    VOGEL_DISK,

    COUNT
};


struct GPU_ShadowFilterData
{
    property bool isRandRotationEnabled
    {
        get { return (bool)randRotationEnabled; }
    }

    GPU_PCFType pcfType;
    float radius;       // grid half size if pcfType - GRID
                        // disk radius if pcfType - POISSON_DISK or VOGEL_DISK
    uint samplesCount;
    
    private uint randRotationEnabled : 1;
    private uint bitPadding : 31;
};


struct GPU_PcssData
{
    property bool isEnabled
    {
        get { return (bool)enabled; }
    }

    float penumbraGrowth;
    float maxPenumbraRadius;
    float maxSearchRadius;
    float minPenumbraRadiusInTexels;

    private uint2 padding;
    uint searchSamplesCount;

    private uint enabled : 1;
    private uint bitPadding : 31;
};


struct GPU_CommonCsmData
{
    property bool isEnabled
    {
        get { return (bool)enabled; }
    }

    property bool isSplitBlendEnabled
    {
        get { return (bool)splitBlendEnabled; }
    }

    GPU_PcssData pcssData;
    GPU_ShadowFilterData filterData;

    float blendThresholdCoef;
    float constantBiasTexels;
    float slopeBiasTexels;

    private uint enabled : 1;
    private uint splitBlendEnabled : 1;
    private uint bitPadding : 30;
};


struct GPU_CommonLightingData
{
    property bool isIBLEnabled
    {
        get { return (bool)IBLEnabled; }
    }

    GPU_CommonCsmData csm;

    private uint3 padding;

    private uint IBLEnabled : 1;
    private uint bitPadding : 31;
};


#endif