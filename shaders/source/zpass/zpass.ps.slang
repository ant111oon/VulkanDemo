#include "pass/zpass/vsout.slang"

#include "registers/common_registers.slang"
#include "registers/zpass_registers.slang"

#include "common/math/color_space.slang"


bool NeedDiscard(in uint instID, in float2 uv)
{
    const uint mtlIdx = COMMON_INST_BUFFER[instID].MATERIAL_IDX;
    const COMMON_MATERIAL material = COMMON_MATERIALS[mtlIdx];

    float4 albedo = ONEF4;

    if (material.ALBEDO_TEX_IDX < 0) {
        albedo = SAMPLE_ARR_TEX(COMMON_DBG_TEXTURES, COMMON_DBG_TEX_IDX::WHITE, GET_COMMON_SAMPLER(COMMON_SAMPLER_IDX::LINEAR_REPEAT), uv);
    } else {
        albedo = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, material.ALBEDO_TEX_IDX, GET_COMMON_SAMPLER(COMMON_SAMPLER_IDX::LINEAR_REPEAT), uv);
        albedo.rgb = SRGBToLinear(albedo.rgb);
    }

    albedo *= material.ALBEDO_MULT;

    return albedo.a < material.ALPHA_REF;
}


struct ZPASS_PS_OUTPUT
{
};


[shader("fragment")]
ZPASS_PS_OUTPUT main(ZPASS_VS_OUTPUT input)
{
    ZPASS_PS_OUTPUT result = (ZPASS_PS_OUTPUT)0;

    if (ZPASS_IS_AKILL_PASS) {
        // Move this code under define for example COMMON_AKILL or to separate shader
        if (NeedDiscard(input.flatData.instID, input.data.uv)) {
            discard;
        }
    }

    return result;
}