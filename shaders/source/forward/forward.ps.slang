#include "registers/common_registers.slang"

#include "common/math/color_utils.slang"
#include "common/dbg_vis.slang"
#include "common/common_defines.slang"
#include "common/common_structs.slang"

#include "pass/forward/vsout.slang"


struct FORWARD_PS_OUTPUT
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
FORWARD_PS_OUTPUT main(FORWARD_VS_OUTPUT input)
{
    FORWARD_PS_OUTPUT result = (FORWARD_PS_OUTPUT)0;

    input.data.lnorm = normalize(input.data.lnorm);
    input.data.tangent.xyz = normalize(input.data.tangent.xyz);

    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[input.flatData.instID];

    const float3x3 TBN = CalcTBN(input.data.tangent.xyz, input.data.lnorm);
    COMMON_SURFACE surf = SampleCommonSurface(instInfo.MATERIAL_IDX, TBN, input.data.uv);

    const COMMON_MATERIAL material = COMMON_MATERIALS[instInfo.MATERIAL_IDX];

    if (IS_COMMON_MATERIAL_ALPHA_KILL(material)) {
        if (surf.albedo.a < material.ALPHA_REF) {
            discard;
        }
    }

    const float4x4 wMatr = COMMON_TRANSFORMS[instInfo.TRANSFORM_IDX];
    const float3 wnorm = normalize(mul(wMatr, float4(surf.lnorm, 0.f)).xyz);

    const float4 color = surf.albedo;
    // TODO: color = CalcLighting()

    result.color = color;

    DbgVisOutput(result.color, surf.albedo, wnorm, surf.metalness, surf.roughness, surf.ao, surf.emissive, input.data.lnorm, input.data.tangent);

    return result;
}