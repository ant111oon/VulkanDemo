#include "common/common_defines.slang"

#include "registers/deferred_lighting_registers.slang"
#include "registers/common_registers.slang"

#include "common/lighting/brdf.slang"


struct GBUFFER
{
    float3 albedo;
    float3 normal;
    float metalness;
    float roughness;
    float ao;
};


struct POINT_LIGHT
{
    float3 color;
    float3 wpos;
};


static const uint G_LIGHTS_COUNT = 4;

static POINT_LIGHT G_LIGHTS[G_LIGHTS_COUNT] =
{
    { 25.f * float3(1.f, 0.f, 0.f), float3(-2.f, 1.f, -1.f) },
    { 25.f * float3(0.f, 1.f, 0.f), float3( 2.f, 1.f, -1.f) },
    { 25.f * float3(0.f, 0.f, 1.f), float3( 2.f, 1.f,  1.f) },
    { 25.f * float3(1.f, 1.f, 1.f), float3(-2.f, 1.f,  1.f) },
};


float2 PixelCoordToUV(in uint2 pixelCoord)
{
    return float2(pixelCoord) / float2(max(COMMON_SCREEN_SIZE, ONEU2));
}


float CalcLightAttenuation(in float3 surfWPos, in float3 lightWPos)
{
    const float dist = distance(surfWPos, lightWPos);
    
    return rcp(dist * dist);
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

    return gbuffer;
}


float3 ProcessLight(in GBUFFER gbuffer, in POINT_LIGHT light, in float3 wpos, in float NdotV, in float NDF, in float3 F, in float3 kD)
{
    const float3 lightDir = normalize(light.wpos - wpos);
    const float NdotL = saturate(dot(gbuffer.normal, lightDir));

    const float G = GeometrySmith(NdotV, NdotL, gbuffer.roughness);

    const float3 specular = NDF * G * F / max(4.f * NdotV * NdotL, M3D_EPS);

    const float attenuation = CalcLightAttenuation(wpos, light.wpos);
    const float3 radiance = light.color * attenuation;

    return (kD * gbuffer.albedo / M3D_PI + specular) * radiance * NdotL;
}


float3 ComputeLighting(in uint2 pixelCoord)
{
    const int3 texCoord = int3(pixelCoord, 0);

    GBUFFER gbuffer = SampleGBuffer(texCoord);

#ifdef ENV_DEBUG
    if (!COMMON_DBG_VIS_NONE) {
        return gbuffer.albedo.rgb;
    }
#endif

    const float3 wpos = WPosFromDepth(PixelCoordToUV(pixelCoord), LOAD_TEX(DEFERRED_LIGHTING_DEPTH, texCoord).x, COMMON_CB.INV_VIEW_PROJ_MATRIX);
    const float3 viewDir = normalize(COMMON_CAM_WPOS - wpos);

    const float3 H = normalize(viewDir + gbuffer.normal);

    const float VdotH = saturate(dot(viewDir, H));
    const float NdotH = saturate(dot(gbuffer.normal, H));
    const float NdotV = saturate(dot(gbuffer.normal, viewDir));

    const float3 F0 = lerp(FLOAT3(0.04f), gbuffer.albedo, gbuffer.metalness);
    const float3 F = FresnelSchlick(VdotH, F0);

    const float NDF = DistributionGGX(NdotH, gbuffer.roughness);

    const float3 kD = (ONEF3 - F) * (1.f - gbuffer.metalness);

    float3 Lo = ZEROF3;
    for (uint i = 0; i < G_LIGHTS_COUNT; ++i) {
        Lo += ProcessLight(gbuffer, G_LIGHTS[i], wpos, NdotV, NDF, F, kD);
    }

    const float3 ambient = FLOAT3(0.01f) * gbuffer.albedo * gbuffer.ao;

    return ambient + Lo;
}


[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    if (any(DTid >= COMMON_SCREEN_SIZE)) {
        return;
    }

    const uint2 pixelCoord = uint2(DTid.x, COMMON_SCREEN_SIZE.y - 1 - DTid.y);

    DEFERRED_LIGHTING_OUTPUT_UAV[pixelCoord] = float4(ComputeLighting(pixelCoord), 1.f);
}