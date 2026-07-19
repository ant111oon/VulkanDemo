#include "registers/common_registers.slang"
#include "registers/deferred_lighting_registers.slang"

#include "common/system/sys_constants.slang"

#include "common/brdf.slang"
#include "common/PCF_kernels.slang"

#include "pass/deferred_lighting/vsout.slang"


static const float CSM_MIN_BIAS = 0.0001f;
static const float CSM_MAX_BIAS = 0.01f;
static const float CSM_CONSTANT_BIAS = 0.0001f;
static const float CSM_SLOPE_BIAS = 0.0005f;

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
    return albedo * SAMPLE_TEX_LEVEL(DEFERRED_LIGHTING_IRRADIANCE_MAP, GET_COMMON_SAMPLER(COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE), normal, 0).rgb;
}


float3 GetIndirectPrefiltColor(in float3 viewDir, in float3 normal, in float roughness)
{
    const float3 reflectDir = reflect(-viewDir, normal);
    const float mip = roughness * float(COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1);

    return SAMPLE_TEX_LEVEL(DEFERRED_LIGHTING_PREFILTERED_ENV_MAP, GET_COMMON_SAMPLER(COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE), reflectDir, mip).rgb;
}


float2 GetIndirectBRDFScaleBias(in float NdotV, in float roughness)
{
    return SAMPLE_TEX_LEVEL(DEFERRED_LIGHTING_BRDF_LUT, GET_COMMON_SAMPLER(COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE), float2(NdotV, roughness), 0).rg;
}


float3 ProcessIBL(in GBUFFER gbuffer, in float3 V, in float3 F0)
{
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


float CalcCsmUnfilteredVisibility(in SamplerState smp, in float3 texCoord, in float cmpValue)
{
    float shadow = SAMPLE_TEX_LEVEL(DEFERRED_LIGHTING_CSM, smp, texCoord, 0).r;

#ifdef ENV_REVERSED_Z
    shadow = shadow < cmpValue ? 1.f : 0.f;
#else
    shadow = shadow > cmpValue ? 1.f : 0.f;
#endif

    return shadow;
}


uint HashUint(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

uint HashUint3(uint3 v)
{
    uint h = HashUint(v.x);
    h = HashUint(h ^ v.y);
    h = HashUint(h ^ v.z);
    return h;
}

float HashUint3ToFloat(uint3 v)
{
    return float(HashUint3(v)) * rcp(4294967296.0f);
}


float2x2 GetCsmPoissonRandRotMatr(in float3 wpos)
{
    float2x2 matr = float2x2(
        1.f, 0.f,
        0.f, 1.f
    );

    if (COMMON_DBG_IS_CSM_POISSON_RANDOM_OFFSETS_ENABLED) {
        const float theta = HashUint3ToFloat(asuint(wpos)) * M3D_2_PI;

        matr = float2x2(float2(cos(theta), -sin(theta)),
                        float2(sin(theta),  cos(theta)));
    }

    return matr;
}


float CalcCsmPoissonPCF(in SamplerComparisonState sampler, in float cmpValue, in float3 texCoord, in float3 wpos)
{
    uint width;
    uint height;
    uint layers;
    DEFERRED_LIGHTING_CSM.GetDimensions(width, height, layers);

    const float2 radiusTexel = ONEF2 / float2(width, height) * COMMON_CSM_POISSON_DISK_RADIUS;
    const float2x2 randRotMatr = GetCsmPoissonRandRotMatr(wpos);

    float shadow = 0.f;

    for (uint i = 0; i < COMMON_CSM_POISSON_DISK_SAMPLE_COUNT; ++i) {
        float2 offset = POISSON_DISK_SAMPLES[i];

        if (COMMON_DBG_IS_CSM_POISSON_RANDOM_OFFSETS_ENABLED) {
            offset = mul(offset, randRotMatr);
        }

        offset *= radiusTexel;

        shadow += SAMPLE_TEX_CMP_ZERO(DEFERRED_LIGHTING_CSM, sampler, float3(texCoord.xy + offset, texCoord.z), cmpValue);
    }

    return shadow / COMMON_CSM_POISSON_DISK_SAMPLE_COUNT;
}


float CalcCsmPCF(in float3 wpos, in float viewZ, in float bias, in float3 texCoord)
{
#ifdef ENV_REVERSED_Z
    const SamplerComparisonState cmpSmp = GET_COMMON_CMP_SAMPLER(COMMON_CMP_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER_GREATER);
    const float cmpValue = viewZ + bias;
#else
    const SamplerComparisonState cmpSmp = GET_COMMON_CMP_SAMPLER(COMMON_CMP_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER_LESS);
    const float cmpValue = viewZ - bias;
#endif

    const SamplerState smp = GET_COMMON_SAMPLER(COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_BORDER);

    float shadow = 0.f;

    switch (COMMON_DBG_CSM_PCF_PRESET) {
        case DBG_CSM_PCF_PRESET::PCF_NONE:
            shadow = CalcCsmUnfilteredVisibility(smp, texCoord, cmpValue);
            break;

        case DBG_CSM_PCF_PRESET::PCF_2X2:
            shadow += SAMPLE_TEX_CMP_ZERO(DEFERRED_LIGHTING_CSM, cmpSmp, texCoord, cmpValue);
            break;

        case DBG_CSM_PCF_PRESET::PCF_3X3:
            for (int y = -1; y <= 1; ++y) {
                for (int x = -1; x <= 1; ++x) {
                    shadow += SAMPLE_TEX_CMP_ZERO_OFFSET(DEFERRED_LIGHTING_CSM, cmpSmp, texCoord, cmpValue, int2(x, y));
                }
            }
            
            shadow *= (1.f / 9.f);
            break;

        case DBG_CSM_PCF_PRESET::PCF_POISSON_DISK:
            shadow = CalcCsmPoissonPCF(cmpSmp, cmpValue, texCoord, wpos);
            break;

        default:
            shadow = 1.f;
            break;
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

    const float shadow = CalcCsmPCF(wpos, ndc.z, bias, float3(uv, cascade));

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

    CSM_CASCADE_BLEND_DATA blendData;

    if (COMMON_DBG_IS_CSM_CASCADE_BLEND_ENABLED) {
        blendData = GetCsmCascadeBlendData(vDist, cascadeIdx);

        if (blendData.NeedBlend()) {
            const float3 nearCascadeShadow = CalcCsmShadowCascade(blendData.closestCascade, wpos, bias);

            shadow = lerp(shadow, nearCascadeShadow, blendData.blendCoef);
        }
    }

    const float3 cascadeColor = GetCsmCascadeColor(cascadeIdx, blendData);

    return shadow * cascadeColor;
}


float3 ComputeLighting(in int2 pixelCoord)
{
    const int3 texCoord = int3(pixelCoord, 0);

    GBUFFER gbuffer = SampleGBuffer(texCoord);

    const float depth = LOAD_TEX(DEFERRED_LIGHTING_DEPTH, texCoord).x;

    const float2 uv = PixelCoordToUV(pixelCoord, COMMON_SCREEN_SIZE);
    const float3 wpos = WPosFromDepth(uv, depth, COMMON_CB.INV_VIEW_MATRIX, COMMON_CB.INV_PROJ_MATRIX);
    const float3 vpos = (mul(COMMON_CB.VIEW_MATRIX, float4(wpos, 1.f))).xyz;

    const float3 V = normalize(COMMON_CAM_WPOS - wpos);
    const float3 F0 = lerp(COMMON_DIELECTRIC_F0, gbuffer.albedo, gbuffer.metalness);

    float3 sunLightResult = ProcessLight(gbuffer, V, COMMON_SUN_LIGHT_DIRECTION, COMMON_SUN_LIGHT_COLOR.rgb, F0);
    sunLightResult *= CalcCsmShadow(wpos, vpos, gbuffer.normal);

    float3 result = ZEROF3;
    result += sunLightResult;

    // for (uint i = 0; i < G_POINT_LIGHTS_COUNT; ++i) {
    //     const float3 surfToLight = wpos - G_POINT_LIGHTS[i].wpos;
    //     const float dist         = length(surfToLight);
    //     const float3 lightDir    = surfToLight / max(dist, M3D_EPS);

    //     LIGHTING_RESULT ptRes = ProcessLight(gbuffer, V, lightDir, G_POINT_LIGHTS[i].color, F0);
    //     ptRes.Lo *= CalcLightAttenuation(dist, G_POINT_LIGHTS[i].radius);

    //     result += ptRes;
    // }

    if (COMMON_DBG_USE_IBL) {
        result += ProcessIBL(gbuffer, V, F0);
    }

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