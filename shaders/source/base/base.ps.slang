#include "registers/common_registers.slang"
#include "math/colors.slang"

#include "base/vsout.slang"


struct BASE_PS_OUTPUT
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
BASE_PS_OUTPUT main(BASE_VS_OUTPUT input)
{
    BASE_PS_OUTPUT result = (BASE_PS_OUTPUT)0;

    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[input.flatData.instID];
    const COMMON_MATERIAL material = COMMON_MATERIALS[instInfo.MATERIAL_IDX];

    result.color = float4(0.f, 0.f, 0.f, 1.f);

    const SamplerState smp = COMMON_SAMPLERS[(uint)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT];

    if (COMMON_DBG_IS_OUTPUT_COMMON_MTL_ALBEDO_TEX) {
        if (material.ALBEDO_TEX_IDX >= 0) {
            const float4 albedo = COMMON_MTL_TEXTURES[material.ALBEDO_TEX_IDX].Sample(smp, input.data.texCoord);
            result.color = float4(SRGBToLinear(albedo.rgb), albedo.a);
        }
    } else if (COMMON_DBG_IS_OUTPUT_COMMON_MTL_NORMAL_TEX) { 
        if (material.NORMAL_TEX_IDX >= 0) {
            result.color = float4(COMMON_MTL_TEXTURES[material.NORMAL_TEX_IDX].Sample(smp, input.data.texCoord).rgb, 1.f);
        }
    } else if (COMMON_DBG_IS_OUTPUT_COMMON_MTL_MR_TEX) {
        if (material.MR_TEX_IDX >= 0) {
            result.color = float4(0.f, COMMON_MTL_TEXTURES[material.MR_TEX_IDX].Sample(smp, input.data.texCoord).gb, 1.f);
        }
    } else if (COMMON_DBG_IS_OUTPUT_COMMON_MTL_AO_TEX) {
        if (material.AO_TEX_IDX >= 0) {
            result.color = float4(COMMON_MTL_TEXTURES[material.AO_TEX_IDX].Sample(smp, input.data.texCoord).rrr, 1.f);
        }
    } else if (COMMON_DBG_IS_OUTPUT_COMMON_MTL_EMISSIVE_TEX) {
        if (material.EMISSIVE_TEX_IDX >= 0) {
            result.color = COMMON_MTL_TEXTURES[material.EMISSIVE_TEX_IDX].Sample(smp, input.data.texCoord);
        }
    }

    if (result.color.a <= 0.0001f) {
        discard;
    }
    
    return result;
}