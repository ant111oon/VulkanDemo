#include "registers/gbuffer_registers.slang"

#include "common/math/utils.slang"
#include "common/dbg_vis.slang"
#include "common/resource_defines.slang"
#include "common/common_structs.slang"

#include "pass/gbuffer/vsout.slang"


struct GBUFFER_PS_OUTPUT
{
    [[vk::location(0)]] float4 rt0;
    [[vk::location(1)]] float4 rt1;
    [[vk::location(2)]] float4 rt2;
    [[vk::location(3)]] float4 rt3;
};


[shader("fragment")]
GBUFFER_PS_OUTPUT main(GBUFFER_VS_OUTPUT input)
{
    GBUFFER_PS_OUTPUT result = (GBUFFER_PS_OUTPUT)0;

    input.data.lnorm = normalize(input.data.lnorm);
    input.data.tangent.xyz = normalize(input.data.tangent.xyz);

    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[input.flatData.instID];

    const COMMON_MATERIAL material = COMMON_MATERIALS[instInfo.MATERIAL_IDX];

    const float3x3 TBN = CalcTBN(input.data.tangent.xyz, input.data.lnorm);
    COMMON_SURFACE surf = SampleCommonSurface(material, TBN, input.data.uv);

    const float4x4 wMatr = COMMON_TRANSFORMS[instInfo.TRANSFORM_IDX];
    const float3 wnorm = normalize(mul(wMatr, float4(surf.lnorm, 0.f)).xyz);

    result.rt0 = surf.albedo;
    result.rt1 = float4(wnorm, 0.f);
    result.rt2 = float4(surf.metalness, surf.roughness, surf.ao, 0.f);
    result.rt3 = float4(surf.emissive, 0.f);

#ifdef ENV_DEBUG
    DbgVisOutput(result.rt0, surf.albedo, wnorm, surf.metalness, surf.roughness, surf.ao, surf.emissive, input.data.lnorm, input.data.tangent);
#endif

    return result;
}