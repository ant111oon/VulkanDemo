#include "registers/common_registers.slang"
#include "registers/dbg_rt_view_registers.slang"

#include "common/system/sys_constants.slang"

#include "common/math/common.slang"
#include "common/tonemapping.slang"

#include "pass/dbg_rt_view/vsout.slang"


void DepthView(out float4 color, in uint2 pixCoord)
{
    const float depth = LOAD_TEX(DBG_RT_VIEW_COMMON_DEPTH, int3(pixCoord, 0)).r;
    const float linearDepth = LinearizeDepth01(depth, DBG_RT_DEPTH_Z_NEAR, DBG_RT_DEPTH_Z_FAR);

    color = float4(FLOAT3(linearDepth), 1.f);
}


void HZBView(out float4 color, in uint2 pixCoord, in uint mip)
{
    const float2 uv = PixelCoordToUV(pixCoord, COMMON_SCREEN_SIZE);

    const float depth = SAMPLE_TEX_LEVEL(DBG_RT_VIEW_COMMON_HZB, GET_COMMON_SAMPLER(GPU_CommonSamplerID::NEAREST_CLAMP_TO_EDGE), uv, mip).r;
    const float linearDepth = LinearizeDepth01(depth, DBG_RT_DEPTH_Z_NEAR, DBG_RT_DEPTH_Z_FAR);

    color = float4(FLOAT3(linearDepth), 1.f);
}


void GBufferAlbedoView(out float4 color, in uint2 pixCoord)
{
    const float3 albedo = LOAD_TEX(DBG_RT_VIEW_GBUFFER_0, int3(pixCoord, 0)).rgb;

    color = float4(albedo, 1.f);
}


void GBufferNormalView(out float4 color, in uint2 pixCoord)
{
    const float3 normal = normalize(LOAD_TEX(DBG_RT_VIEW_GBUFFER_1, int3(pixCoord, 0)).xyz);

    color = float4(normal, 1.f);
}


void GBufferRoughnessView(out float4 color, in uint2 pixCoord)
{
    const float roughness = LOAD_TEX(DBG_RT_VIEW_GBUFFER_2, int3(pixCoord, 0)).g;

    color = float4(FLOAT3(roughness), 1.f);
}


void GBufferMetalnessView(out float4 color, in uint2 pixCoord)
{
    const float metalness = LOAD_TEX(DBG_RT_VIEW_GBUFFER_2, int3(pixCoord, 0)).r;

    color = float4(FLOAT3(metalness), 1.f);
}


void GBufferAOView(out float4 color, in uint2 pixCoord)
{
    const float ao = LOAD_TEX(DBG_RT_VIEW_GBUFFER_2, int3(pixCoord, 0)).b;

    color = float4(FLOAT3(ao), 1.f);
}


void GBufferEmissiveView(out float4 color, in uint2 pixCoord)
{
    const float3 emissive = LOAD_TEX(DBG_RT_VIEW_GBUFFER_3, int3(pixCoord, 0)).rgb;

    color = float4(emissive, 1.f);
}


void BRDF_LUTView(out float4 color, in uint2 pixCoord)
{
    const float2 uv = PixelCoordToUV(pixCoord, COMMON_SCREEN_SIZE);
    const float2 brdf = SAMPLE_TEX_LEVEL(DBG_RT_VIEW_BRDF_LUT, GET_COMMON_SAMPLER(GPU_CommonSamplerID::LINEAR_CLAMP_TO_EDGE), uv, 0).rg;

    color = float4(brdf, 0.f, 1.f);
}


void IrradianceMapView(out float4 color, in uint2 pixCoord, in uint face, in uint mip)
{
    const float2 uv = PixelCoordToUV(pixCoord, COMMON_SCREEN_SIZE);
    const float3 dir = UVFaceToDir(uv, face);

    const float3 irr = SAMPLE_TEX_LEVEL(DBG_RT_VIEW_IRRADIANCE_MAP, GET_COMMON_SAMPLER(GPU_CommonSamplerID::LINEAR_CLAMP_TO_EDGE), dir, mip).rgb;

    color = float4(irr, 1.f);
}


void PrefiltEnvMapView(out float4 color, in uint2 pixCoord, in uint face, in uint mip)
{
    const float2 uv = PixelCoordToUV(pixCoord, COMMON_SCREEN_SIZE);
    const float3 dir = UVFaceToDir(uv, face);

    const float3 env = SAMPLE_TEX_LEVEL(DBG_RT_VIEW_PREFILTERED_ENV_MAP, GET_COMMON_SAMPLER(GPU_CommonSamplerID::LINEAR_CLAMP_TO_EDGE), dir, mip).rgb;

    color = float4(env, 1.f);
}


void SkyboxView(out float4 color, in uint2 pixCoord, in uint face, in uint mip)
{
    const float2 uv = PixelCoordToUV(pixCoord, COMMON_SCREEN_SIZE);
    const float3 dir = UVFaceToDir(uv, face);

    const float3 skybox = SAMPLE_TEX_LEVEL(DBG_RT_VIEW_SKYBOX, GET_COMMON_SAMPLER(GPU_CommonSamplerID::LINEAR_CLAMP_TO_EDGE), dir, mip).rgb;

    color = float4(skybox, 1.f);
}


void CSMView(out float4 color, in uint2 pixCoord, in uint cascade)
{
    const float2 uv = PixelCoordToUV(pixCoord, COMMON_SCREEN_SIZE);
    const uint index = clamp(cascade, 0, COMMON_CSM_CASCADE_COUNT - 1);

    const float depth = SAMPLE_TEX_LEVEL(DBG_RT_VIEW_CSM, GET_COMMON_SAMPLER(GPU_CommonSamplerID::LINEAR_CLAMP_TO_EDGE), float3(uv, index), 0).r;

    color = float4(FLOAT3(depth), 1.f);
}


struct GPU_DbgRTViewPsOutput
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
GPU_DbgRTViewPsOutput main(GPU_DbgRTViewVsOutput input, uint2 pixCoord : SV_Position)
{
    GPU_DbgRTViewPsOutput result = (GPU_DbgRTViewPsOutput)0;

    switch (COMMON_DBG_RT_VIEW_TYPE) {
        case GPU_DbgRTViewType::NONE:
            break;
        case GPU_DbgRTViewType::COMMON_DEPTH:
            DepthView(result.color, pixCoord);
            break;
        case GPU_DbgRTViewType::COMMON_HZB:
            HZBView(result.color, pixCoord, DBG_RT_VIEW_MIP);
            break;
        case GPU_DbgRTViewType::GBUFFER_ALBEDO:
            GBufferAlbedoView(result.color, pixCoord);
            break;
        case GPU_DbgRTViewType::GBUFFER_NORMAL:
            GBufferNormalView(result.color, pixCoord);
            break;
        case GPU_DbgRTViewType::GBUFFER_ROUGHNESS:
            GBufferRoughnessView(result.color, pixCoord);
            break;
        case GPU_DbgRTViewType::GBUFFER_METALNESS:
            GBufferMetalnessView(result.color, pixCoord);
            break;
        case GPU_DbgRTViewType::GBUFFER_AO:
            GBufferAOView(result.color, pixCoord);
            break;
        case GPU_DbgRTViewType::GBUFFER_EMISSIVE:
            GBufferEmissiveView(result.color, pixCoord);
            break;
        case GPU_DbgRTViewType::IRRADIANCE_MAP:
            IrradianceMapView(result.color, pixCoord, DBG_RT_VIEW_FACE, DBG_RT_VIEW_MIP);
            break;
        case GPU_DbgRTViewType::PREFILTERED_ENV_MAP:
            PrefiltEnvMapView(result.color, pixCoord, DBG_RT_VIEW_FACE, DBG_RT_VIEW_MIP);
            break;
        case GPU_DbgRTViewType::BRDF_LUT:
            BRDF_LUTView(result.color, pixCoord);
            break;
        case GPU_DbgRTViewType::SKYBOX:
            SkyboxView(result.color, pixCoord, DBG_RT_VIEW_FACE, DBG_RT_VIEW_MIP);
            break;
        case GPU_DbgRTViewType::CSM_DEPTH:
            CSMView(result.color, pixCoord, DBG_RT_CSM_CASCADE_IDX);
            break;
    }

    return result;
}