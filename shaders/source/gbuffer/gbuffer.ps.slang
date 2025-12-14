#include "registers/common_registers.slang"
#include "math/color_utils.slang"

#include "common/dbg_vis.slang"
#include "common/common_defines.slang"

#include "pass/gbuffer/common.slang"
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

    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[input.flatData.instID];
    const COMMON_TRANSFORM transform = COMMON_TRANSFORMS[instInfo.TRANSFORM_IDX];

    const float3x4 wMatr = float3x4(transform.MATR[0], transform.MATR[1], transform.MATR[2]);

    const float3x3 TBN = float3x3(
        1.f, 0.f, 0.f,
        0.f, 1.f, 0.f,
        0.f, 0.f, 1.f
    );
    COMMON_SURFACE surf = SampleCommonSurface(instInfo.MATERIAL_IDX, TBN, wMatr, input.data.uv);

    if (surf.albedo.a <= 0.0001f) {
        discard;
    }

    result.rt0 = surf.albedo;
    result.rt1 = float4(surf.wnorm, 0.f);
    result.rt2 = float4(surf.metalness, surf.roughness, surf.ao, 0.f);
    result.rt3 = surf.emissive;

    DbgVisOutput(result.rt0, surf.albedo, surf.lnorm, surf.metalness, surf.roughness, surf.ao, surf.emissive, input.data.lnorm);

    return result;
}