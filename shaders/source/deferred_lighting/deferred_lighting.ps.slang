#include "registers/common_registers.slang"
#include "registers/deferred_lighting_registers.slang"

#include "common/system/sys_constants.slang"

#include "common/brdf.slang"
#include "common/PCF_kernels.slang"

#include "common/math/hash.slang"
#include "common/math/common.slang"

#include "pass/deferred_lighting/vsout.slang"


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


struct GBUFFER
{
    float3 albedo;
    float metalness;
    float3 normal;
    float roughness;
    float3 emissive;
    float ao;
};


struct POINT_LIGHT
{
    float3 color;
    float3 wpos;
    float radius;
};


// static const uint G_POINT_LIGHTS_COUNT = 4;

// static POINT_LIGHT G_POINT_LIGHTS[G_POINT_LIGHTS_COUNT] =
// {
//     { 8.f * float3(1.f, 0.f, 0.f), float3(-2.f, 2.f, -2.f), 3.f },
//     { 8.f * float3(0.f, 1.f, 0.f), float3( 2.f, 2.f, -2.f), 3.f },
//     { 8.f * float3(0.f, 0.f, 1.f), float3( 2.f,-2.f, -2.f), 3.f },
//     { 8.f * float3(1.f, 0.f, 1.f), float3(-2.f,-2.f, -2.f), 3.f },

//     // { 2.f * ONEF3, float3(-2.f, 2.f, 1.5f), 3.f },
//     // { 2.f * ONEF3, float3( 2.f, 2.f, 1.5f), 3.f },
//     // { 2.f * ONEF3, float3( 2.f,-2.f, 1.5f), 3.f },
//     // { 2.f * ONEF3, float3(-2.f,-2.f, 1.5f), 3.f },

//     // { 2.f * ONEF3, float3(-2.f, 2.f, -4.5f), 3.f },
//     // { 2.f * ONEF3, float3( 2.f, 2.f, -4.5f), 3.f },
//     // { 2.f * ONEF3, float3( 2.f,-2.f, -4.5f), 3.f },
//     // { 2.f * ONEF3, float3(-2.f,-2.f, -4.5f), 3.f },
// };


float CalcLightAttenuation(in float distToLight, in float lightRadius)
{
    float t = distToLight / max(lightRadius, M3D_EPS);
    t = saturate(1.f - t * t * t * t);
    t = t * t;

    return t / (distToLight * distToLight + 1.f);
}


GBUFFER SampleGBuffer(in int3 texCoord)
{
    GBUFFER gbuffer = (GBUFFER)0;

    gbuffer.albedo = LOAD_TEX(DEFERRED_LIGHTING_GBUFFER_0, texCoord).rgb;
    gbuffer.normal = normalize(LOAD_TEX(DEFERRED_LIGHTING_GBUFFER_1, texCoord).xyz);
    
    const float3 metRoughAo = LOAD_TEX(DEFERRED_LIGHTING_GBUFFER_2, texCoord).rgb;

    gbuffer.metalness = metRoughAo.r;
    gbuffer.roughness = metRoughAo.g;
    gbuffer.ao = metRoughAo.b;

    gbuffer.emissive = LOAD_TEX(DEFERRED_LIGHTING_GBUFFER_3, texCoord).rgb;

    return gbuffer;
}


float3 ProcessLight(in GBUFFER gbuffer, in float3 V, in float3 lightDir, in float3 lightColor, in float3 F0)
{
    const float3 L = -lightDir;
    const float3 H = normalize(V + L);

    const float VdotH = saturate(dot(V, H));
    const float NdotL = saturate(dot(gbuffer.normal, L));
    const float NdotH = saturate(dot(gbuffer.normal, H));
    const float NdotV = saturate(dot(gbuffer.normal, V));

    const float3 F = F_Schlick(VdotH, F0);

    const float D = NDF_GGX(NdotH, gbuffer.roughness);

    const float k = CalcDirectK(gbuffer.roughness);
    const float G = G_Smith(NdotV, NdotL, k);

    const float3 specular = (D * G * F) / max(4.f * NdotV * NdotL, M3D_EPS);

    const float3 radiance = lightColor * NdotL;

    const float3 kD = CalcDiffuseCoef(F, gbuffer.metalness);
    const float3 diffuse = kD * (gbuffer.albedo / M3D_PI);

    const float3 result = (diffuse + specular) * radiance;

    return result;
}


float3 GetIndirectDiffuse(in float3 albedo, in float3 normal)
{
    return albedo * SAMPLE_TEX_LEVEL(DEFERRED_LIGHTING_IRRADIANCE_MAP, GET_COMMON_SAMPLER(GPU_CommonSamplerID::LINEAR_CLAMP_TO_EDGE), normal, 0).rgb;
}


float3 GetIndirectPrefiltColor(in float3 viewDir, in float3 normal, in float roughness)
{
    const float3 reflectDir = reflect(-viewDir, normal);
    const float mip = roughness * float(COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1);

    return SAMPLE_TEX_LEVEL(DEFERRED_LIGHTING_PREFILTERED_ENV_MAP, GET_COMMON_SAMPLER(GPU_CommonSamplerID::LINEAR_CLAMP_TO_EDGE), reflectDir, mip).rgb;
}


float2 GetIndirectBRDFScaleBias(in float NdotV, in float roughness)
{
    return SAMPLE_TEX_LEVEL(DEFERRED_LIGHTING_BRDF_LUT, GET_COMMON_SAMPLER(GPU_CommonSamplerID::LINEAR_CLAMP_TO_EDGE), float2(NdotV, roughness), 0).rg;
}


float3 ProcessIBL(in GBUFFER gbuffer, in float3 V, in float3 F0)
{
    if (!COMMON_DBG_USE_IBL) {
        return gbuffer.albedo * gbuffer.ao * 0.015f;
    }

    const float NdotV = saturate(dot(gbuffer.normal, V));

    const float3 kS = F_SchlickRoughness(NdotV, F0, gbuffer.roughness);
    const float3 kD = CalcDiffuseCoef(kS, gbuffer.metalness);

    const float3 diffuse          = GetIndirectDiffuse(gbuffer.albedo, gbuffer.normal);
    const float3 prefilteredColor = GetIndirectPrefiltColor(V, gbuffer.normal, gbuffer.roughness);
    const float2 brdfScaleBias    = GetIndirectBRDFScaleBias(NdotV, gbuffer.roughness);

    const float3 specular = prefilteredColor * (kS * brdfScaleBias.x + brdfScaleBias.y);

    const float3 ambient = (kD * diffuse + specular) * gbuffer.ao;

    return ambient;
}


uint GetCsmCascadeIndex(in float viewZ)
{
    for (uint cascade = 0; cascade < COMMON_CSM_CASCADE_COUNT; ++cascade) {
        if (viewZ < COMMON_CSM_CASCADE_DISTANCE(cascade)) {
            return cascade;
        }
    }

    return COMMON_CSM_CASCADE_COUNT;
}


struct CSM_CASCADE_BLEND_DATA
{
    __init()
    {
        closestCascade = COMMON_CSM_CASCADE_COUNT;
        blendCoef = 0.f;
    }

    bool NeedBlend() { return closestCascade != COMMON_CSM_CASCADE_COUNT; }

    uint closestCascade;
    float blendCoef;
};


CSM_CASCADE_BLEND_DATA GetCsmCascadeBlendData(in float viewZ, in uint currCascadeIndex)
{
    const float2 bounds = COMMON_CSM_CASCADE_BOUNDS(currCascadeIndex);
    const float cascadeSize = bounds[1] - bounds[0];

    const uint closestBoundIdx = abs(viewZ - bounds[0]) < abs(viewZ - bounds[1]) ? 0 : 1;

    const float bound = bounds[closestBoundIdx];
    const float thresholdCoef = abs(bound - viewZ) / cascadeSize;

    const uint rawBoundIdx = currCascadeIndex + closestBoundIdx;

    bool needBlend = rawBoundIdx > 0 && rawBoundIdx < COMMON_CSM_CASCADE_BOUNDS_COUNT - 1;
    needBlend = needBlend && (thresholdCoef <= COMMON_CSM_CASCADE_BLEND_THRESHOLD_COEF);

    if (!needBlend) {
        return CSM_CASCADE_BLEND_DATA();
    }

    const uint closestCascadeIdx = currCascadeIndex + (closestBoundIdx == 0 ? -1 : 1);

    const float2 closestCascadeBounds = COMMON_CSM_CASCADE_BOUNDS(closestCascadeIdx);
    const float closestCascadeSize = closestCascadeBounds[1] - closestCascadeBounds[0];

    const float leftThresholdDist = (closestBoundIdx == 0 ? closestCascadeSize : cascadeSize) * COMMON_CSM_CASCADE_BLEND_THRESHOLD_COEF;
    const float rightThresholdDist = (closestBoundIdx == 0 ? cascadeSize : closestCascadeSize) * COMMON_CSM_CASCADE_BLEND_THRESHOLD_COEF;

    CSM_CASCADE_BLEND_DATA result;

    result.closestCascade = closestCascadeIdx;
    result.blendCoef = smoothstep(bound - leftThresholdDist, bound + rightThresholdDist, viewZ);

    if (closestBoundIdx == 0) {
        result.blendCoef = 1.f - result.blendCoef;
    }

    return result;
}


float CalcCsmUnfilteredPCF(in float3 texCoord, in float zReceiver)
{
    const float zBlocker = SAMPLE_TEX_LEVEL(DEFERRED_LIGHTING_CSM, GET_COMMON_SAMPLER(GPU_CommonSamplerID::LINEAR_CLAMP_TO_BORDER), texCoord, 0).r;

#ifdef ENV_REVERSED_Z
    return zBlocker < zReceiver ? 1.f : 0.f;
#else
    return zBlocker > zReceiver ? 1.f : 0.f;
#endif
}


float CalcCsmGridPCF(in SamplerComparisonState cmpSmp, in float3 texCoord, in float zReceiver, in uint halfGridSize)
{
    [branch]
    if (halfGridSize == 0) {
        return SAMPLE_TEX_CMP_ZERO(DEFERRED_LIGHTING_CSM, cmpSmp, texCoord, zReceiver);
    }

    const float2 texelSize = COMMON_CSM_INV_TEX_SIZE;

    const int radius = halfGridSize;

    const int sampleCount = (radius * 2 + 1) * (radius * 2 + 1);
    const float invSampleCount = rcp((float)sampleCount);

    float shadow = 0.f;

    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            const float3 uv = float3(texCoord.xy + float2(x, y) * texelSize, texCoord.z);
            
            shadow += SAMPLE_TEX_CMP_ZERO(DEFERRED_LIGHTING_CSM, cmpSmp, uv, zReceiver);
        }
    }

    return shadow * invSampleCount;
}


float2x2 GetCsmRandRotMatr(in float3 texCoord, in uint salt)
{
    const uint2 textureSize = COMMON_CSM_TEX_SIZE;
    const uint2 maxTexel = textureSize - 1u;

    const uint2 shadowTexel = min(
        uint2(floor(saturate(texCoord.xy) * textureSize)), 
        maxTexel
    );

    const uint cascade = uint(texCoord.z);

    const float theta = Hash(uint3(shadowTexel, cascade ^ salt)) / float(M3D_UINT32_MAX) * M3D_2_PI;

    float sinTheta, cosTheta;
    sincos(theta, sinTheta, cosTheta);

    return float2x2(float2(cosTheta, -sinTheta),
                    float2(sinTheta, cosTheta));
}


float CalcCsmDiskPCF(
    in SamplerComparisonState smSmp,
    in float3 texCoord,
    in float zReceiver,
    in float diskRadius,
    in uint sampleCount,
    in bool randRotEnabled, 
    in IFunc<float2, uint> GetOffsetFunc
) {
    const float2 radiusTexel = COMMON_CSM_INV_TEX_SIZE * diskRadius;
    const float2x2 randRotMatr = randRotEnabled ? GetCsmRandRotMatr(texCoord, CSM_PCSS_FILTER_ROTATION_SALT) : float2x2(1.f, 0.f, 0.f, 1.f);

    const uint cascade = uint(texCoord.z);

    float shadow = 0.f;
    uint validSamplesCount = 0;

    for (uint i = 0; i < sampleCount; ++i) {
        float2 offset = GetOffsetFunc(i);

        if (randRotEnabled) {
            offset = mul(offset, randRotMatr);
        }

        offset *= radiusTexel;

        const float2 uv = texCoord.xy + offset;

        if (any(uv < ZEROF2) || any(uv > ONEF2)) {
            continue;
        }

        shadow += SAMPLE_TEX_CMP_ZERO(DEFERRED_LIGHTING_CSM, smSmp, float3(uv, cascade), zReceiver);
        ++validSamplesCount;
    }

    return validSamplesCount > 0 ? shadow / validSamplesCount : 1.f;
}


float CalcCsmPoissonPCF(in SamplerComparisonState smSmp, in float3 texCoord, in float zReceiver, in float diskRadius, in uint sampleCount, in bool randRotEnabled)
{
    return CalcCsmDiskPCF(smSmp, texCoord, zReceiver, diskRadius, sampleCount, randRotEnabled,
        (uint i) => { return POISSON_DISK_SAMPLES[i]; }
    );
}


float2 VogelDiskSample(int i, int n)
{
    if (i == 0u || n <= 1u) {
        return 0.f;
    }

    i -= 1;
    n -= 1;

    static const float GOLDEN_ANGLE = 2.39996323f;

    const float r = sqrt((float(i) + 0.5f) / float(n));
    const float theta = float(i) * GOLDEN_ANGLE;

    float sinTheta, cosTheta;
    sincos(theta, sinTheta, cosTheta);

    return r * float2(cosTheta, sinTheta);
}


float CalcCsmVogelPCF(in SamplerComparisonState smSmp, in float3 texCoord, in float zReceiver, in float diskRadius, in uint sampleCount, in bool randRotEnabled)
{
    return CalcCsmDiskPCF(smSmp, texCoord, zReceiver, diskRadius, sampleCount, randRotEnabled,
        (uint i) => { return VogelDiskSample(i, sampleCount); }
    );
}


struct CSM_PCSS_BLOCKER_SEARCH_RESULT
{
    bool HasBlocker() { return blockerCount > 0; }

    float avgZViewSpace;
    uint blockerCount;
};


CSM_PCSS_BLOCKER_SEARCH_RESULT FindCsmPCSSAverageBlocker(in float3 texCoord, in float zReceiverViewSpace, in float radius, in bool randRotEnabled)
{
    const float2 searchRadius = COMMON_CSM_INV_TEX_SIZE * radius;

    const float2x2 randRotMatr = randRotEnabled ? GetCsmRandRotMatr(texCoord, CSM_PCSS_SEARCH_ROTATION_SALT) : float2x2(1.f, 0.f, 0.f, 1.f);

    const uint cascade = uint(texCoord.z);

    float avgBlocker = 0.f;
    uint blockerCount = 0;

    const SamplerState smp = GET_COMMON_SAMPLER(GPU_CommonSamplerID::NEAREST_CLAMP_TO_BORDER);
    const uint sampleCount = COMMON_CSM_PCSS_SEARCH_SAMPLES_COUNT;

    for (uint i = 0; i < sampleCount; ++i) {
        float2 offset = VogelDiskSample(i, sampleCount);

        if (randRotEnabled) {
            offset = mul(offset, randRotMatr);
        }
        offset *= searchRadius;

        const float2 uv = texCoord.xy + offset;

        if (any(uv < ZEROF2) || any(uv > ONEF2)) {
            continue;
        }

        const float zBlocker = SAMPLE_TEX_LEVEL(DEFERRED_LIGHTING_CSM, smp, float3(uv, cascade), 0).r;
        const float zBlockerViewSpace = OrthoDepthToViewDepth(zBlocker, COMMON_CSM_CASCADE_Z_NEAR(cascade), COMMON_CSM_CASCADE_Z_FAR(cascade));

        if (zBlockerViewSpace < zReceiverViewSpace) {
            avgBlocker += zBlockerViewSpace;
            ++blockerCount;
        }
    }

    CSM_PCSS_BLOCKER_SEARCH_RESULT result;

    result.blockerCount = blockerCount;
    result.avgZViewSpace = blockerCount > 0u ? avgBlocker / blockerCount : 0.f;

    return result;
}


float CalcCSM_PCSS(in SamplerComparisonState smSmp, in float3 texCoord, in float zReceiver)
{
    const uint cascade = uint(texCoord.z);

    const float cascadeZNear = COMMON_CSM_CASCADE_Z_NEAR(cascade);
    const float zReceiverViewSpace = OrthoDepthToViewDepth(zReceiver, cascadeZNear, COMMON_CSM_CASCADE_Z_FAR(cascade));

    const float lightSlope = COMMON_CSM_PCSS_LIGHT_ANGULAR_SLOPE;

    // A conservative blocker-search radius.
    // A blocker can be located anywhere between the cascade near plane and the receiver.
    // The largest possible lateral displacement is therefore based on that maximum distance.
    const float zReceiverFromNear = max(zReceiverViewSpace - cascadeZNear, 0.f);
    const float searchRadiusWorld = lightSlope * zReceiverFromNear * COMMON_CSM_PCSS_SEARCH_RADIUS_SCALE;

    const float worldUnitsPerTexel = max(COMMON_CSM_CASCADE_WORLD_UNITS_PER_TEXEL(cascade), M3D_EPS);
    const float searchRadiusTexels = min(searchRadiusWorld / worldUnitsPerTexel, COMMON_CSM_PCSS_MAX_SEARCH_RADIUS_TEXELS);

    const CSM_PCSS_BLOCKER_SEARCH_RESULT blockers = FindCsmPCSSAverageBlocker(texCoord, zReceiverViewSpace, searchRadiusTexels, COMMON_DBG_IS_CSM_PCSS_RAND_ROT_ENABLED);

    if (!blockers.HasBlocker()) {
        return 1.f;
    }

    const float blockerReceiverDistance = zReceiverViewSpace - blockers.avgZViewSpace;

    if (blockerReceiverDistance <= 0.f) {
        return 1.f;
    }

    const float penumbraWorld = blockerReceiverDistance * lightSlope * COMMON_CSM_PCSS_FILTER_RADIUS_SCALE;
    const float filterRadiusTexels = min(penumbraWorld / worldUnitsPerTexel, COMMON_CSM_PCSS_MAX_FILTER_RADIUS_TEXELS);

    // Very small penumbras do not justify running a full sample filter.
    if (filterRadiusTexels < COMMON_CSM_PCSS_MIN_FILTER_RADIUS_TEXELS) {
        return SAMPLE_TEX_CMP_ZERO(DEFERRED_LIGHTING_CSM, smSmp, texCoord, zReceiver);
    }

    return CalcCsmVogelPCF(smSmp, texCoord, zReceiver, filterRadiusTexels, COMMON_CSM_PCSS_FILTER_SAMPLES_COUNT, COMMON_DBG_IS_CSM_PCSS_RAND_ROT_ENABLED);
}


float CalcCsmFilteredShadow(in float viewZ, in float bias, in float3 texCoord)
{
#ifdef ENV_REVERSED_Z
    const SamplerComparisonState cmpSmp = GET_COMMON_CMP_SAMPLER(GPU_CommonCmpSamplerID::LINEAR_CLAMP_TO_BORDER_GREATER);
    const float zReceiver = viewZ + bias;
#else
    const SamplerComparisonState cmpSmp = GET_COMMON_CMP_SAMPLER(GPU_CommonCmpSamplerID::LINEAR_CLAMP_TO_BORDER_LESS);
    const float zReceiver = viewZ - bias;
#endif

    float shadow = 0.f;

    if (COMMON_DBG_IS_CSM_PCSS_ENABLED) {
        shadow = CalcCSM_PCSS(cmpSmp, texCoord, zReceiver);
    } else {    
        switch (COMMON_DBG_CSM_PCF_PRESET) {
            case GPU_DbgCsmPCFPreset::NONE:
                shadow = CalcCsmUnfilteredPCF(texCoord, zReceiver);
                break;
            case GPU_DbgCsmPCFPreset::GRID:
                shadow += CalcCsmGridPCF(cmpSmp, texCoord, zReceiver, COMMON_CSM_FILTER_GRID_HALF_SIZE);
                break;
            case GPU_DbgCsmPCFPreset::POISSON_DISK:
                shadow = CalcCsmPoissonPCF(cmpSmp, texCoord, zReceiver, COMMON_CSM_FILTER_DISK_RADIUS, COMMON_CSM_FILTER_DISK_SAMPLE_COUNT, COMMON_DBG_IS_CSM_FILTER_RANDOM_OFFSETS_ENABLED);
                break;
            case GPU_DbgCsmPCFPreset::VOGEL_DISK:
                shadow = CalcCsmVogelPCF(cmpSmp, texCoord, zReceiver, COMMON_CSM_FILTER_DISK_RADIUS, COMMON_CSM_FILTER_DISK_SAMPLE_COUNT, COMMON_DBG_IS_CSM_FILTER_RANDOM_OFFSETS_ENABLED);
                break;
            default:
                shadow = 1.f;
                break;
        }
    }

    return shadow;
}


float3 CalcCsmShadowCascade(in uint cascade, in float3 wpos, in float bias)
{
    if (cascade >= COMMON_CSM_CASCADE_COUNT) {
        return ONEF3;
    }

    const float4 ndc = mul(COMMON_CSM_VIEW_PROJ_MATRIX(cascade), float4(wpos, 1.f));
    const float2 uv = NDCToUV(ndc.xy);

    const float shadow = CalcCsmFilteredShadow(ndc.z, bias, float3(uv, cascade));

    return FLOAT3(shadow);
}


float3 GetCsmCascadeColor(in uint currCascadeIdx, in CSM_CASCADE_BLEND_DATA blendData)
{
    if (!COMMON_DBG_IS_CSM_VISUALIZATION_ENABLED) {
        return ONEF3;
    }

    float3 cascadeColor = DBG_CSM_CASCADE_COLORS[currCascadeIdx];

    if (blendData.NeedBlend()) {
        cascadeColor = lerp(cascadeColor, DBG_CSM_CASCADE_COLORS[blendData.closestCascade], blendData.blendCoef);
    }

    return cascadeColor;
}


float CalcCsmBias(in float3 wnorm)
{
    const float NdotL = saturate(dot(wnorm, -COMMON_SUN_LIGHT_DIRECTION));

    // tan(acos(x)) = sqrt(1 - x²) / x
    const float sinTheta = sqrt(max(1.f - NdotL * NdotL, 0.f));

    const float tanTheta = sinTheta / max(NdotL, M3D_EPS);

    const float bias = CSM_CONSTANT_BIAS + CSM_SLOPE_BIAS * tanTheta;

    return clamp(bias, CSM_MIN_BIAS, CSM_MAX_BIAS);
}


float3 ApplyCSMCascadeBlend(inout float3 shadow, in float3 wpos, in float vDist, in float bias, in uint currCascadeIndex)
{
    CSM_CASCADE_BLEND_DATA blendData;

    if (COMMON_DBG_IS_CSM_CASCADE_BLEND_ENABLED) {
        blendData = GetCsmCascadeBlendData(vDist, currCascadeIndex);

        if (blendData.NeedBlend()) {
            const float3 nearCascadeShadow = CalcCsmShadowCascade(blendData.closestCascade, wpos, bias);

            shadow = lerp(shadow, nearCascadeShadow, blendData.blendCoef);
        }
    }

    return GetCsmCascadeColor(currCascadeIndex, blendData);
}


float3 CalcCsmShadow(in float3 wpos, in float3 vpos, in float3 wnorm)
{
    if (!COMMON_DBG_IS_CSM_ENABLED) {
        return ONEF3;
    }

    const float vDist = abs(vpos.z);
    const uint cascadeIdx = GetCsmCascadeIndex(vDist);

    if (cascadeIdx >= COMMON_CSM_CASCADE_COUNT) {
        return ONEF3;
    }

    const float bias = CalcCsmBias(wnorm);
    float3 shadow = CalcCsmShadowCascade(cascadeIdx, wpos, bias);

    const float3 cascadeDbgColor = ApplyCSMCascadeBlend(shadow, wpos, vDist, bias, cascadeIdx);

    return shadow * cascadeDbgColor;
}


float3 ComputeLighting(in int2 pixelCoord)
{
    const int3 texCoord = int3(pixelCoord, 0);

    GBUFFER gbuffer = SampleGBuffer(texCoord);

    const float depth = LOAD_TEX(DEFERRED_LIGHTING_DEPTH, texCoord).x;

    const float2 uv = PixelCoordToUV(pixelCoord, COMMON_SCREEN_SIZE);
    const float3 wpos = WPosFromDepth(uv, depth, COMMON_MAIN_CAM_INV_VIEW_MATRIX, COMMON_MAIN_CAM_INV_PROJ_MATRIX);
    const float3 vpos = mul(COMMON_MAIN_CAM_VIEW_MATRIX, float4(wpos, 1.f)).xyz;

    const float3 V = normalize(COMMON_MAIN_CAM_WPOS - wpos);
    const float3 F0 = lerp(COMMON_DIELECTRIC_F0, gbuffer.albedo, gbuffer.metalness);

    float3 sunLightResult = ProcessLight(gbuffer, V, COMMON_SUN_LIGHT_DIRECTION, COMMON_SUN_LIGHT_COLOR.rgb, F0);
    sunLightResult *= CalcCsmShadow(wpos, vpos, gbuffer.normal);

    float3 result = ProcessIBL(gbuffer, V, F0);
    result += sunLightResult;

    // for (uint i = 0; i < G_POINT_LIGHTS_COUNT; ++i) {
    //     const float3 surfToLight = wpos - G_POINT_LIGHTS[i].wpos;
    //     const float dist         = length(surfToLight);
    //     const float3 lightDir    = surfToLight / max(dist, M3D_EPS);

    //     LIGHTING_RESULT ptRes = ProcessLight(gbuffer, V, lightDir, G_POINT_LIGHTS[i].color, F0);
    //     ptRes.Lo *= CalcLightAttenuation(dist, G_POINT_LIGHTS[i].radius);

    //     result += ptRes;
    // }

    result += gbuffer.emissive;

    return result;
}


struct DEFERRED_LIGHTING_PS_OUTPUT
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
DEFERRED_LIGHTING_PS_OUTPUT main(DEFERRED_LIGHTING_VS_OUTPUT input, uint2 pixCoord : SV_Position)
{
    DEFERRED_LIGHTING_PS_OUTPUT result = (DEFERRED_LIGHTING_PS_OUTPUT)0;

    result.color = float4(ComputeLighting(pixCoord), 1.f);

    return result;
}