#ifndef COMMON_LIGHTING_H
#define COMMON_LIGHTING_H

#include "common/math/geom_primitives.slang"
#include "brdf.slang"


static const uint CSM_CASCADE_COUNT = 3;
static const uint CSM_CASCADE_BOUNDS_COUNT = CSM_CASCADE_COUNT + 1;

static const float CSM_MIN_BIAS = 0.0001f;
static const float CSM_MAX_BIAS = 0.01f;
static const float CSM_CONSTANT_BIAS = 0.0001f;
static const float CSM_SLOPE_BIAS = 0.0005f;

static const uint CSM_PCSS_SEARCH_ROTATION_SALT = 0x68bc21ebu;
static const uint CSM_PCSS_FILTER_ROTATION_SALT = 0x02e5be93u;

static const float3 DBG_CSM_CASCADE_COLORS[] = {
    float3(1.f, 0.f, 0.f),
    float3(0.f, 1.f, 0.f),
    float3(0.f, 0.f, 1.f),
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


struct GPU_CsmCascadeData
{
    GPU_Frustum frustum;
    float4x4 viewMatr;
    float4x4 viewProjMatr;

    float zNear;
    float zFar;
    float distance;
    float worldUnitsPerPixel;
};


struct GPU_CsmData
{
    GPU_CsmCascadeData cascades[CSM_CASCADE_COUNT];

    GPU_PcssData pcssData;

    float blendThresholdCoef;
    uint  filterDiskSampleCount;
    float filterDiskRadius;
    uint  filterGridHalfSize;

    uint2 rtSize;
    uint2 padding;
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