#include "registers/common_registers.slang"
#include "registers/deferred_lighting_registers.slang"

#include "common/system/sys_constants.slang"

#include "common/common_lighting.slang"
#include "common/PCF_kernels.slang"

#include "common/math/hash.slang"
#include "common/math/common.slang"

#include "pass/deferred_lighting/vsout.slang"


struct GPU_GBuffer
{
    float3 albedo;
    float metalness;
    float3 normal;
    float roughness;
    float3 emissive;
    float ao;
};


// struct GPU_PointLight
// {
//     float3 color;
//     float3 wpos;
//     float radius;
// };
//
// static const uint G_POINT_LIGHTS_COUNT = 4;
//
// static GPU_PointLight G_POINT_LIGHTS[G_POINT_LIGHTS_COUNT] =
// {
//     { 8.f * float3(1.f, 0.f, 0.f), float3(-2.f, 2.f, -2.f), 3.f },
//     { 8.f * float3(0.f, 1.f, 0.f), float3( 2.f, 2.f, -2.f), 3.f },
//     { 8.f * float3(0.f, 0.f, 1.f), float3( 2.f,-2.f, -2.f), 3.f },
//     { 8.f * float3(1.f, 0.f, 1.f), float3(-2.f,-2.f, -2.f), 3.f },
//
//     // { 2.f * ONEF3, float3(-2.f, 2.f, 1.5f), 3.f },
//     // { 2.f * ONEF3, float3( 2.f, 2.f, 1.5f), 3.f },
//     // { 2.f * ONEF3, float3( 2.f,-2.f, 1.5f), 3.f },
//     // { 2.f * ONEF3, float3(-2.f,-2.f, 1.5f), 3.f },
//
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


GPU_GBuffer SampleGBuffer(in int3 texCoord)
{
    GPU_GBuffer gbuffer = (GPU_GBuffer)0;

    gbuffer.albedo = LOAD_TEX(DEFERRED_LIGHTING_GBUFFER_0, texCoord).rgb;
    gbuffer.normal = normalize(LOAD_TEX(DEFERRED_LIGHTING_GBUFFER_1, texCoord).xyz);
    
    const float3 metRoughAo = LOAD_TEX(DEFERRED_LIGHTING_GBUFFER_2, texCoord).rgb;

    gbuffer.metalness = metRoughAo.r;
    gbuffer.roughness = metRoughAo.g;
    gbuffer.ao = metRoughAo.b;

    gbuffer.emissive = LOAD_TEX(DEFERRED_LIGHTING_GBUFFER_3, texCoord).rgb;

    return gbuffer;
}


float3 ProcessLight(in GPU_GBuffer gbuffer, in float3 V, in float3 lightDir, in float3 lightColor, in float3 F0)
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
    return albedo * SAMPLE_TEX_LEVEL(DEFERRED_LIGHTING_IRRADIANCE_MAP, GET_COMMON_SMP(GPU_SamplerID::LINEAR_CLAMP_TO_EDGE), normal, 0).rgb;
}


float3 GetIndirectPrefiltColor(in float3 viewDir, in float3 normal, in float roughness)
{
    const float3 reflectDir = reflect(-viewDir, normal);
    const float mip = roughness * float(COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1);

    return SAMPLE_TEX_LEVEL(DEFERRED_LIGHTING_PREFILTERED_ENV_MAP, GET_COMMON_SMP(GPU_SamplerID::LINEAR_CLAMP_TO_EDGE), reflectDir, mip).rgb;
}


float2 GetIndirectBRDFScaleBias(in float NdotV, in float roughness)
{
    return SAMPLE_TEX_LEVEL(DEFERRED_LIGHTING_BRDF_LUT, GET_COMMON_SMP(GPU_SamplerID::LINEAR_CLAMP_TO_EDGE), float2(NdotV, roughness), 0).rg;
}


float3 ProcessIBL(in GPU_GBuffer gbuffer, in float3 V, in float3 F0)
{
    if (!COMMON_DBG_IS_IBL_ENABLED) {
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
    for (uint cascade = 0; cascade < CSM_CASCADE_COUNT; ++cascade) {
        if (viewZ < DEFERRED_LIGHTING_CSM_CASCADE_DISTANCE(cascade)) {
            return cascade;
        }
    }

    return CSM_CASCADE_COUNT;
}


float2 GetCsmCascadeBounds(in uint cascade)
{
    return float2(
        cascade == 0 ? 0.f : DEFERRED_LIGHTING_CSM_CASCADE_DISTANCE(cascade - 1),
        DEFERRED_LIGHTING_CSM_CASCADE_DISTANCE(cascade)
    );
}


struct GPU_CsmCascadeBlendData
{
    __init()
    {
        closestCascade = CSM_CASCADE_COUNT;
        blendCoef = 0.f;
    }

    bool NeedBlend() { return closestCascade != CSM_CASCADE_COUNT; }

    uint closestCascade;
    float blendCoef;
};


GPU_CsmCascadeBlendData GetCsmCascadeBlendData(in float viewZ, in uint cascade)
{
    const float2 bounds = GetCsmCascadeBounds(cascade);
    const float cascadeSize = bounds[1] - bounds[0];

    const uint closestBoundIdx = abs(viewZ - bounds[0]) < abs(viewZ - bounds[1]) ? 0 : 1;

    const float bound = bounds[closestBoundIdx];
    const float thresholdCoef = abs(bound - viewZ) / cascadeSize;

    const uint rawBoundIdx = cascade + closestBoundIdx;

    const float blendThresoldCoef = DEFERRED_LIGHTING_CSM_DATA.blendThresholdCoef;

    bool needBlend = rawBoundIdx > 0 && rawBoundIdx < CSM_CASCADE_BOUNDS_COUNT - 1;
    needBlend = needBlend && (thresholdCoef <= blendThresoldCoef);

    if (!needBlend) {
        return GPU_CsmCascadeBlendData();
    }

    const uint closestCascade = cascade + (closestBoundIdx == 0 ? -1 : 1);

    const float2 closestCascadeBounds = GetCsmCascadeBounds(closestCascade);
    const float closestCascadeSize = closestCascadeBounds[1] - closestCascadeBounds[0];

    const float leftThresholdDist = (closestBoundIdx == 0 ? closestCascadeSize : cascadeSize) * blendThresoldCoef;
    const float rightThresholdDist = (closestBoundIdx == 0 ? cascadeSize : closestCascadeSize) * blendThresoldCoef;

    GPU_CsmCascadeBlendData result;

    result.closestCascade = closestCascade;
    result.blendCoef = smoothstep(bound - leftThresholdDist, bound + rightThresholdDist, viewZ);

    if (closestBoundIdx == 0) {
        result.blendCoef = 1.f - result.blendCoef;
    }

    return result;
}


float CalcCsmUnfilteredPCF(in float3 texCoord, in float zReceiverCmp)
{
    const float zBlocker = SAMPLE_TEX_LEVEL(DEFERRED_LIGHTING_CSM, GET_COMMON_SMP(GPU_SamplerID::LINEAR_CLAMP_TO_BORDER), texCoord, 0).r;

#ifdef ENV_REVERSED_Z
    return zBlocker < zReceiverCmp ? 1.f : 0.f;
#else
    return zBlocker > zReceiverCmp ? 1.f : 0.f;
#endif
}


float CalcCsmGridPCF(in SamplerComparisonState cmpSmp, in float3 texCoord, in float zReceiverCmp, in uint halfGridSize)
{
    [branch]
    if (halfGridSize == 0) {
        return SAMPLE_TEX_CMP_ZERO(DEFERRED_LIGHTING_CSM, cmpSmp, texCoord, zReceiverCmp);
    }

    const int radius = halfGridSize;

    const int sampleCount = (radius * 2 + 1) * (radius * 2 + 1);
    const float invSampleCount = rcp((float)sampleCount);

    const float2 texelSize = DEFERRED_LIGHTING_CSM_INV_RT_SIZE;

    float shadow = 0.f;

    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            const float3 uv = float3(texCoord.xy + float2(x, y) * texelSize, texCoord.z);
            
            shadow += SAMPLE_TEX_CMP_ZERO(DEFERRED_LIGHTING_CSM, cmpSmp, uv, zReceiverCmp);
        }
    }

    return shadow * invSampleCount;
}


float2x2 GetCsmRandRotMatr(in float3 texCoord, in uint salt)
{
    const uint2 textureSize = DEFERRED_LIGHTING_CSM_RT_SIZE;
    const uint2 maxTexel = textureSize - ONEU2;

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
    in float zReceiverCmp,
    in float diskRadius,
    in uint sampleCount,
    in bool randRotEnabled, 
    in IFunc<float2, uint> GetOffsetFunc
) {
    const float2 radiusTexel = DEFERRED_LIGHTING_CSM_INV_RT_SIZE * diskRadius;
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

        shadow += SAMPLE_TEX_CMP_ZERO(DEFERRED_LIGHTING_CSM, smSmp, float3(uv, cascade), zReceiverCmp);
        ++validSamplesCount;
    }

    return validSamplesCount > 0 ? shadow / validSamplesCount : 1.f;
}


float CalcCsmPoissonPCF(in SamplerComparisonState smSmp, in float3 texCoord, in float zReceiverCmp, in float diskRadius, in uint sampleCount, in bool randRotEnabled)
{
    return CalcCsmDiskPCF(smSmp, texCoord, zReceiverCmp, diskRadius, sampleCount, randRotEnabled,
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


float CalcCsmVogelPCF(in SamplerComparisonState smSmp, in float3 texCoord, in float zReceiverCmp, in float diskRadius, in uint sampleCount, in bool randRotEnabled)
{
    return CalcCsmDiskPCF(smSmp, texCoord, zReceiverCmp, diskRadius, sampleCount, randRotEnabled,
        (uint i) => { return VogelDiskSample(i, sampleCount); }
    );
}


struct GPU_CsmPcssBlockerSearchResult
{
    bool HasBlocker() { return blockerCount > 0; }

    float avgZViewSpace;
    uint blockerCount;
};


GPU_CsmPcssBlockerSearchResult FindCsmPCSSAverageBlocker(in float3 texCoord, in float zReceiverViewSpace, in float radius, in bool randRotEnabled)
{
    const float2 searchRadius = DEFERRED_LIGHTING_CSM_INV_RT_SIZE * radius;

    const float2x2 randRotMatr = randRotEnabled ? GetCsmRandRotMatr(texCoord, CSM_PCSS_SEARCH_ROTATION_SALT) : float2x2(1.f, 0.f, 0.f, 1.f);

    const uint cascade = uint(texCoord.z);

    float avgBlocker = 0.f;
    uint blockerCount = 0;

    const SamplerState smp = GET_COMMON_SMP(GPU_SamplerID::NEAREST_CLAMP_TO_BORDER);
    const uint sampleCount = DEFERRED_LIGHTING_CSM_PCSS_DATA.searchSamplesCount;

    const float zNear = DEFERRED_LIGHTING_CSM_CASCADE_Z_NEAR(cascade);
    const float zFar = DEFERRED_LIGHTING_CSM_CASCADE_Z_FAR(cascade);

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
        const float zBlockerViewSpace = OrthoDepthToViewDepth(zBlocker, zNear, zFar);

        if (zBlockerViewSpace < zReceiverViewSpace) {
            avgBlocker += zBlockerViewSpace;
            ++blockerCount;
        }
    }

    GPU_CsmPcssBlockerSearchResult result;

    result.blockerCount = blockerCount;
    result.avgZViewSpace = blockerCount > 0u ? avgBlocker / blockerCount : 0.f;

    return result;
}


float CalcCSM_PCSS(in SamplerComparisonState smSmp, in float3 texCoord, in float zReceiverRaw, in float zReceiverCmp)
{
    const uint cascade = uint(texCoord.z);

    const float zNear = DEFERRED_LIGHTING_CSM_CASCADE_Z_NEAR(cascade);
    const float zFar = DEFERRED_LIGHTING_CSM_CASCADE_Z_FAR(cascade);
    const float zReceiverViewSpace = OrthoDepthToViewDepth(zReceiverRaw, zNear, zFar);

    const float lightSlope = DEFERRED_LIGHTING_CSM_PCSS_DATA.lightAngularSlope;

    // A conservative blocker-search radius.
    // A blocker can be located anywhere between the cascade near plane and the receiver.
    // The largest possible lateral displacement is therefore based on that maximum distance.
    const float zReceiverFromNear = max(zReceiverViewSpace - zNear, 0.f);
    const float searchRadiusWorld = lightSlope * zReceiverFromNear * DEFERRED_LIGHTING_CSM_PCSS_DATA.searchRadiusScale;

    const float worldUnitsPerTexel = max(DEFERRED_LIGHTING_CSM_CASCADE_WORLD_UNITS_PER_PIXEL(cascade), M3D_EPS);
    const float searchRadiusTexels = min(searchRadiusWorld / worldUnitsPerTexel, DEFERRED_LIGHTING_CSM_PCSS_DATA.maxSearchRadiusTexels);

    const GPU_CsmPcssBlockerSearchResult blockers = FindCsmPCSSAverageBlocker(texCoord, zReceiverViewSpace, searchRadiusTexels, COMMON_DBG_IS_CSM_PCSS_RAND_ROT_ENABLED);

    if (!blockers.HasBlocker()) {
        return 1.f;
    }

    const float blockerReceiverDistance = zReceiverViewSpace - blockers.avgZViewSpace;

    if (blockerReceiverDistance <= 0.f) {
        return 1.f;
    }

    const float penumbraWorld = blockerReceiverDistance * lightSlope * DEFERRED_LIGHTING_CSM_PCSS_DATA.filterRadiusScale;
    const float filterRadiusTexels = min(penumbraWorld / worldUnitsPerTexel, DEFERRED_LIGHTING_CSM_PCSS_DATA.maxFilterRadiusTexels);

    // Very small penumbras do not justify running a full sample filter.
    if (filterRadiusTexels < DEFERRED_LIGHTING_CSM_PCSS_DATA.minFilterRadiusTexels) {
        return SAMPLE_TEX_CMP_ZERO(DEFERRED_LIGHTING_CSM, smSmp, texCoord, zReceiverCmp);
    }

    return CalcCsmVogelPCF(
        smSmp,
        texCoord,
        zReceiverCmp,
        filterRadiusTexels,
        DEFERRED_LIGHTING_CSM_PCSS_DATA.filterSamplesCount,
        COMMON_DBG_IS_CSM_PCSS_RAND_ROT_ENABLED);
}


float CalcCsmFilteredShadow(in float zReceiverRaw, in float bias, in float3 texCoord)
{
#ifdef ENV_REVERSED_Z
    const SamplerComparisonState cmpSmp = GET_COMMON_CMP_SMP(GPU_CmpSamplerID::LINEAR_CLAMP_TO_BORDER_GREATER);
    const float zReceiverCmp = zReceiverRaw + bias;
#else
    const SamplerComparisonState cmpSmp = GET_COMMON_CMP_SMP(GPU_CmpSamplerID::LINEAR_CLAMP_TO_BORDER_LESS);
    const float zReceiverCmp = zReceiverRaw - bias;
#endif

    float shadow = 0.f;

    if (COMMON_DBG_IS_CSM_PCSS_ENABLED) {
        shadow = CalcCSM_PCSS(cmpSmp, texCoord, zReceiverRaw, zReceiverCmp);
    } else {    
        switch (COMMON_DBG_CSM_PCF_PRESET) {
            case GPU_DbgCsmPCFPreset::NONE:
                shadow = CalcCsmUnfilteredPCF(texCoord, zReceiverCmp);
                break;
            case GPU_DbgCsmPCFPreset::GRID:
                shadow += CalcCsmGridPCF(cmpSmp, texCoord, zReceiverCmp, DEFERRED_LIGHTING_CSM_DATA.filterGridHalfSize);
                break;
            case GPU_DbgCsmPCFPreset::POISSON_DISK:
                shadow = CalcCsmPoissonPCF(
                    cmpSmp,
                    texCoord,
                    zReceiverCmp,
                    DEFERRED_LIGHTING_CSM_DATA.filterDiskRadius,
                    DEFERRED_LIGHTING_CSM_DATA.filterDiskSampleCount, 
                    COMMON_DBG_IS_CSM_FILTER_RAND_OFFSETS_ENABLED
                );
                break;
            case GPU_DbgCsmPCFPreset::VOGEL_DISK:
                shadow = CalcCsmVogelPCF(
                    cmpSmp,
                    texCoord,
                    zReceiverCmp,
                    DEFERRED_LIGHTING_CSM_DATA.filterDiskRadius, 
                    DEFERRED_LIGHTING_CSM_DATA.filterDiskSampleCount, 
                    COMMON_DBG_IS_CSM_FILTER_RAND_OFFSETS_ENABLED);
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
    if (cascade >= CSM_CASCADE_COUNT) {
        return ONEF3;
    }

    const float4 ndc = mul(DEFERRED_LIGHTING_CSM_CASCADE_VIEW_PROJ_MATR(cascade), float4(wpos, 1.f));
    const float2 uv = NDCToUV(ndc.xy);

    const float shadow = CalcCsmFilteredShadow(ndc.z, bias, float3(uv, cascade));

    return FLOAT3(shadow);
}


float3 GetCsmCascadeColor(in uint currCascadeIdx, in GPU_CsmCascadeBlendData blendData)
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

float CalcCsmBias(in float3 wnorm, in uint cascade)
{
    const float NdotL = saturate(dot(wnorm, -DEFERRED_LIGHTING_SUN_DATA.direction));

    const float sinTheta = sqrt(max(1.f - NdotL * NdotL, 0.f));
    const float tanTheta = sinTheta / max(NdotL, M3D_EPS);

    const float worldUnitsPerTexel = DEFERRED_LIGHTING_CSM_CASCADE_WORLD_UNITS_PER_PIXEL(cascade);

    // Tune these in texel units.
    const float constantBiasTexels = 0.25f;
    const float slopeBiasTexels = 0.5f;

    const float biasWorld = worldUnitsPerTexel * (constantBiasTexels + slopeBiasTexels * tanTheta);

    const float zNear = DEFERRED_LIGHTING_CSM_CASCADE_Z_NEAR(cascade);
    const float zFar = DEFERRED_LIGHTING_CSM_CASCADE_Z_FAR(cascade);

    const float depthRange = abs(zFar - zNear);

    return biasWorld / max(depthRange, M3D_EPS);
}


float3 ApplyCSMCascadeBlend(inout float3 shadow, in float3 wpos, in float vDist, in float bias, in uint currCascadeIndex)
{
    GPU_CsmCascadeBlendData blendData;

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

    if (cascadeIdx >= CSM_CASCADE_COUNT) {
        return ONEF3;
    }

    const float bias = CalcCsmBias(wnorm, cascadeIdx);
    float3 shadow = CalcCsmShadowCascade(cascadeIdx, wpos, bias);

    const float3 cascadeDbgColor = ApplyCSMCascadeBlend(shadow, wpos, vDist, bias, cascadeIdx);

    return shadow * cascadeDbgColor;
}


float3 ComputeLighting(in int2 pixelCoord)
{
    const int3 texCoord = int3(pixelCoord, 0);

    GPU_GBuffer gbuffer = SampleGBuffer(texCoord);

    const float depth = LOAD_TEX(DEFERRED_LIGHTING_DEPTH, texCoord).x;

    const float2 uv = PixelCoordToUV(pixelCoord, COMMON_SCREEN_SIZE);
    const float3 wpos = WPosFromDepth(uv, depth, COMMON_MAIN_CAM_INV_VIEW_MATRIX, COMMON_MAIN_CAM_INV_PROJ_MATRIX);
    const float3 vpos = mul(COMMON_MAIN_CAM_VIEW_MATRIX, float4(wpos, 1.f)).xyz;

    const float3 V = normalize(COMMON_MAIN_CAM_WPOS - wpos);
    const float3 F0 = lerp(COMMON_DIELECTRIC_F0, gbuffer.albedo, gbuffer.metalness);

    float3 sunLightResult = ProcessLight(gbuffer, V, DEFERRED_LIGHTING_SUN_DATA.direction, DEFERRED_LIGHTING_SUN_DATA.color, F0);
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


struct GPU_DeferredLightingPsOutput
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
GPU_DeferredLightingPsOutput main(GPU_DeferredLightingVsOutput input, uint2 pixCoord : SV_Position)
{
    GPU_DeferredLightingPsOutput result = (GPU_DeferredLightingPsOutput)0;

    result.color = float4(ComputeLighting(pixCoord), 1.f);

    return result;
}