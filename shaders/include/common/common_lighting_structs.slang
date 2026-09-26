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


struct GPU_PcssData
{
    float lightAngularSlope;
    float maxSearchRadiusTexels;
    float maxFilterRadiusTexels;
    float minFilterRadiusTexels;

    float searchRadiusScale;
    float filterRadiusScale;
    uint searchSamplesCount;
    uint filterSamplesCount;
};


struct GPU_CsmSplitData
{
    GPU_CommonCameraData camData;

    float2 zBounds;
    float  worldUnitsPerPixel;
    uint   padding;
};


struct GPU_CsmData
{
    GPU_CsmSplitData splits[CSM_SPLIT_COUNT];

    GPU_PcssData pcssData;

    float blendThresholdCoef;
    uint  filterDiskSampleCount;
    float filterDiskRadius;
    uint  filterGridHalfSize;

    uint2 rtSize;
    float constantBiasTexels;
    float slopeBiasTexels;
};


struct GPU_SunData
{
    property float3 color
    {
        get { return unpackUnorm4x8ToFloat(packedColor).rgb; }
    }

    float3 direction;
    
    private uint packedColor;
};


struct GPU_LightingData
{
    GPU_CsmData csmData;
    GPU_SunData sunData;
};


#endif