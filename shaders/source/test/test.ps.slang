import "test_vsout.fx";

import "registers_common.fx";
import "registers_test.fx";


struct TEST_PS_OUTPUT
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
TEST_PS_OUTPUT main(TEST_VS_OUTPUT input)
{
    TEST_PS_OUTPUT result = (TEST_PS_OUTPUT)0;

    const COMMON_RENDER_INFO renderInfo = COMMON_RENDER_INFOS[input.flatData.renderInfoID];
    const COMMON_MATERIAL material = COMMON_MATERIALS[renderInfo.MATERIAL_IDX];

    result.color = float4(0.f, 0.f, 0.f, 1.f);

    const SamplerState smp = COMMON_SAMPLERS[uint(COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT)];

    if ((COMMON_CB.COMMON_DBG_FLAGS & COMMON_DBG_FLAG_MASKS::OUTPUT_COMMON_MTL_ALBEDO_TEX) != 0) {
        if (material.ALBEDO_TEX_IDX >= 0) {
            result.color = COMMON_MTL_TEXTURES[material.ALBEDO_TEX_IDX].Sample(smp, input.data.texCoord);
        }
    } else if ((COMMON_CB.COMMON_DBG_FLAGS & COMMON_DBG_FLAG_MASKS::OUTPUT_COMMON_MTL_NORMAL_TEX) != 0) {
        if (material.NORMAL_TEX_IDX >= 0) {
            result.color = float4(COMMON_MTL_TEXTURES[material.NORMAL_TEX_IDX].Sample(smp, input.data.texCoord).rgb, 1.f);
        }
    } else if ((COMMON_CB.COMMON_DBG_FLAGS & COMMON_DBG_FLAG_MASKS::OUTPUT_COMMON_MTL_MR_TEX) != 0) {
        if (material.MR_TEX_IDX >= 0) {
            result.color = float4(0.f, COMMON_MTL_TEXTURES[material.MR_TEX_IDX].Sample(smp, input.data.texCoord).gb, 1.f);
        }
    } else if ((COMMON_CB.COMMON_DBG_FLAGS & COMMON_DBG_FLAG_MASKS::OUTPUT_COMMON_MTL_AO_TEX) != 0) {
        if (material.AO_TEX_IDX >= 0) {
            result.color = float4(COMMON_MTL_TEXTURES[material.AO_TEX_IDX].Sample(smp, input.data.texCoord).rrr, 1.f);
        }
    } else if ((COMMON_CB.COMMON_DBG_FLAGS & COMMON_DBG_FLAG_MASKS::OUTPUT_COMMON_MTL_EMISSIVE_TEX) != 0) {
        if (material.EMISSIVE_TEX_IDX >= 0) {
            result.color = COMMON_MTL_TEXTURES[material.EMISSIVE_TEX_IDX].Sample(smp, input.data.texCoord);
        }
    }

    if (result.color.a <= 0.0001f) {
        discard;
    }
    
    return result;
}