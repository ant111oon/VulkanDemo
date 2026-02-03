#include "registers/deferred_lighting_registers.slang"
#include "registers/common_registers.slang"

#include "common/lighting/brdf.slang"

#include "common/common_cs.slang"
#include "common/resource_defines.slang"


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


struct DIRECTIONAL_LIGHT
{
    float3 color;
    float3 direction;
};


struct LIGHTING_RESULT
{
    float3 Lo;
};


void operator+=(inout LIGHTING_RESULT a, in LIGHTING_RESULT b)
{
    a.Lo += b.Lo;
}


static const uint G_POINT_LIGHTS_COUNT = 4;

static POINT_LIGHT G_POINT_LIGHTS[G_POINT_LIGHTS_COUNT] =
{
    { 8.f * float3(1.f, 0.f, 0.f), float3(-2.f, 2.f, -2.f), 3.f },
    { 8.f * float3(0.f, 1.f, 0.f), float3( 2.f, 2.f, -2.f), 3.f },
    { 8.f * float3(0.f, 0.f, 1.f), float3( 2.f,-2.f, -2.f), 3.f },
    { 8.f * float3(1.f, 0.f, 1.f), float3(-2.f,-2.f, -2.f), 3.f },

    // { 2.f * ONEF3, float3(-2.f, 2.f, 1.5f), 3.f },
    // { 2.f * ONEF3, float3( 2.f, 2.f, 1.5f), 3.f },
    // { 2.f * ONEF3, float3( 2.f,-2.f, 1.5f), 3.f },
    // { 2.f * ONEF3, float3(-2.f,-2.f, 1.5f), 3.f },

    // { 2.f * ONEF3, float3(-2.f, 2.f, -4.5f), 3.f },
    // { 2.f * ONEF3, float3( 2.f, 2.f, -4.5f), 3.f },
    // { 2.f * ONEF3, float3( 2.f,-2.f, -4.5f), 3.f },
    // { 2.f * ONEF3, float3(-2.f,-2.f, -4.5f), 3.f },
};


static DIRECTIONAL_LIGHT G_DIRECTIONAL_LIGHT = { 2.f * ONEF3, normalize(1.5f * M3D_AXIS_X - M3D_AXIS_Y - M3D_AXIS_Z) };


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


LIGHTING_RESULT ProcessLight(in GBUFFER gbuffer, in float3 V, in float3 lightDir, in float3 lightColor, in float3 F0)
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

    LIGHTING_RESULT result = (LIGHTING_RESULT)0;

    result.Lo = (diffuse + specular) * radiance;

    return result;
}


float3 GetIndirectDiffuse(in float3 albedo, in float3 normal)
{
    return albedo * SAMPLE_TEX_LEVEL(DEFERRED_LIGHTING_IRRADIANCE_MAP, GET_COMMON_SAMPLER(LINEAR_CLAMP_TO_EDGE), normal, 0).rgb;
}


float3 GetIndirectPrefiltColor(in float3 viewDir, in float3 normal, in float roughness)
{
    const float3 reflectDir = reflect(-viewDir, normal);
    const float mip = roughness * float(COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1);

    return SAMPLE_TEX_LEVEL(DEFERRED_LIGHTING_PREFILTERED_ENV_MAP, GET_COMMON_SAMPLER(LINEAR_CLAMP_TO_EDGE), reflectDir, mip).rgb;
}


float2 GetIndirectBRDFScaleBias(in float NdotV, in float roughness)
{
    return SAMPLE_TEX_LEVEL(DEFERRED_LIGHTING_BRDF_LUT, GET_COMMON_SAMPLER(LINEAR_CLAMP_TO_EDGE), float2(NdotV, roughness), 0).rg;
}


LIGHTING_RESULT ProcessIndirectLighting(in GBUFFER gbuffer, in float3 V, in float3 F0)
{
    const float NdotV = saturate(dot(gbuffer.normal, V));

    const float3 kS = F_SchlickRoughness(NdotV, F0, gbuffer.roughness);
    const float3 kD = CalcDiffuseCoef(kS, gbuffer.metalness);

    const float3 diffuse          = GetIndirectDiffuse(gbuffer.albedo, gbuffer.normal);
    const float3 prefilteredColor = GetIndirectPrefiltColor(V, gbuffer.normal, gbuffer.roughness);
    const float2 brdfScaleBias    = GetIndirectBRDFScaleBias(NdotV, gbuffer.roughness);

    const float3 specular = prefilteredColor * (kS * brdfScaleBias.x + brdfScaleBias.y);

    const float3 ambient = (kD * diffuse + specular) * gbuffer.ao;

    LIGHTING_RESULT result = (LIGHTING_RESULT)0;
    result.Lo = ambient;

    return result;
}


float3 ComputeLighting(in int2 pixelCoord)
{
    const int3 texCoord = int3(pixelCoord, 0);

    GBUFFER gbuffer = SampleGBuffer(texCoord);

#ifdef ENV_DEBUG
    if (!COMMON_DBG_VIS_NONE) {
        return gbuffer.albedo.rgb;
    }
#endif

    const float depth = LOAD_TEX(DEFERRED_LIGHTING_DEPTH, texCoord).x;

    const float2 uv = PixelCoordToUV(pixelCoord, COMMON_SCREEN_SIZE);
    const float3 wpos = WPosFromDepth(uv, depth, COMMON_CB.INV_VIEW_MATRIX, COMMON_CB.INV_PROJ_MATRIX);

    const float3 V = normalize(COMMON_CAM_WPOS - wpos);
    const float3 F0 = lerp(COMMON_DIELECTRIC_F0, gbuffer.albedo, gbuffer.metalness);

    LIGHTING_RESULT result = (LIGHTING_RESULT)0;

    result += ProcessLight(gbuffer, V, G_DIRECTIONAL_LIGHT.direction, G_DIRECTIONAL_LIGHT.color, F0);

    for (uint i = 0; i < G_POINT_LIGHTS_COUNT; ++i) {
        const float3 surfToLight = wpos - G_POINT_LIGHTS[i].wpos;
        const float dist         = length(surfToLight);
        const float3 lightDir    = surfToLight / max(dist, M3D_EPS);

        LIGHTING_RESULT ptRes = ProcessLight(gbuffer, V, lightDir, G_POINT_LIGHTS[i].color, F0);
        ptRes.Lo *= CalcLightAttenuation(dist, G_POINT_LIGHTS[i].radius);

        result += ptRes;
    }

#ifdef ENV_DEBUG
    if (COMMON_DBG_USE_INDIRECT_LIGHTING)
#endif
    {
        result += ProcessIndirectLighting(gbuffer, V, F0);
    }

    result.Lo += gbuffer.emissive;

    return result.Lo;
}


[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    if (any(DTid >= COMMON_SCREEN_SIZE)) {
        return;
    }

    const int2 pixCoord = DispatchThreadIDToPixCoord(DTid, COMMON_SCREEN_SIZE);

    DEFERRED_LIGHTING_OUTPUT_UAV[pixCoord] = float4(ComputeLighting(pixCoord), 1.f);
}