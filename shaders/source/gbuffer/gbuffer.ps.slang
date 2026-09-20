#include "registers/gbuffer_registers.slang"
#include "pass/gbuffer/vsout.slang"

#include "common/common_gbuffer.slang"
#include "common/common_geom.slang"


void GBufferDebugVis(in uint instID, inout float4 albedo)
{
#ifdef ENV_DEBUG
    if (COMMON_DBG_IS_GEOM_LOD_VISUALIZATION_ENABLED) {
        const uint lodID = GBUFFER_DBG_CULL_INST_DATA[instID].lodID;
        albedo = float4(GEOM_LOD_VIS_COLORS[lodID], 1.f);
    }
#endif
}


struct GPU_GBufferPsOutput
{
    [[vk::location(0)]] float4 rt0;
    [[vk::location(1)]] float4 rt1;
    [[vk::location(2)]] float4 rt2;
    [[vk::location(3)]] float4 rt3;
};


[shader("fragment")]
GPU_GBufferPsOutput main(GPU_GBufferVsOutput input)
{
    GPU_GBufferPsOutput result = (GPU_GBufferPsOutput)0;

    input.data.lnorm = normalize(input.data.lnorm);
    input.data.tangent.xyz = normalize(input.data.tangent.xyz);

    const GPU_GeomInst inst = COMMON_INST_BUFFER[input.flatData.instID];

    const GPU_GeomMaterial material = COMMON_MATERIALS[inst.materialID];

    const float3x3 TBN = CalcTBN(input.data.tangent, input.data.lnorm);
    GPU_GBufferSurface surf = SampleGBufferSurface(material, TBN, input.data.uv);

    const float3 wnorm = normalize(mul(inst.matrWCS, float4(surf.lnorm, 0.f)).xyz);

    GBufferDebugVis(input.flatData.instID, surf.albedo);

    result.rt0 = surf.albedo;
    result.rt1 = float4(wnorm, 0.f);
    result.rt2 = float4(surf.metalness, surf.roughness, surf.ao, 0.f);
    result.rt3 = float4(surf.emissive, 0.f);

    return result;
}