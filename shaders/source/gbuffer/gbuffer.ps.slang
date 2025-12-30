#include "registers/common_registers.slang"

#include "common/math/color_utils.slang"
#include "common/dbg_vis.slang"
#include "common/common_defines.slang"
#include "common/common_structs.slang"

#include "pass/gbuffer/vsout.slang"


float3x3 CalcTBN(in float3 ltang, in float3 lnorm)
{
    const float3 N = lnorm;
    const float3 T = normalize(ltang - dot(ltang, N) * N);

    const float3 B = normalize(cross(N, T));

    return float3x3(T, B, N);
}


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

    const float3x3 TBN = CalcTBN(input.data.tangent.xyz, input.data.lnorm);

    COMMON_SURFACE surf = SampleCommonSurface(instInfo.MATERIAL_IDX, TBN, input.data.uv);

    const float4x4 wMatr = COMMON_TRANSFORMS[instInfo.TRANSFORM_IDX];
    const float3 wnorm = normalize(mul(wMatr, float4(surf.lnorm, 0.f)).xyz);

    result.rt0 = surf.albedo;
    result.rt1 = float4(wnorm, 0.f);
    result.rt2 = float4(surf.metalness, surf.roughness, surf.ao, 0.f);
    result.rt3 = float4(surf.emissive, 0.f);

    DbgVisOutput(result.rt0, surf.albedo, wnorm, surf.metalness, surf.roughness, surf.ao, surf.emissive, input.data.lnorm, input.data.tangent);

    return result;
}