#include "registers/common_registers.slang"
#include "math/color_utils.slang"

#include "base/vsout.slang"


struct BASE_PS_OUTPUT
{
    [[vk::location(0)]] float4 color;
    [[vk::location(1)]] float4 normal;
};


[shader("fragment")]
BASE_PS_OUTPUT main(BASE_VS_OUTPUT input)
{
    BASE_PS_OUTPUT result = (BASE_PS_OUTPUT)0;

    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[input.flatData.instID];
    const COMMON_MATERIAL material = COMMON_MATERIALS[instInfo.MATERIAL_IDX];

    const SamplerState smp = COMMON_SAMPLERS[(uint)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT];

    result.color = float4(0.f, 0.f, 0.f, 1.f);
    result.normal = float4(0.f, 0.f, 0.f, 1.f);

    if (material.NORMAL_TEX_IDX >= 0) {
        result.normal = float4(normalize(COMMON_MTL_TEXTURES[material.NORMAL_TEX_IDX].Sample(smp, input.data.texCoord).rgb), 1.f);
    }

    if (COMMON_DBG_IS_OUTPUT_COMMON_MTL_ALBEDO_TEX) {
        if (material.ALBEDO_TEX_IDX >= 0) {
            const float4 albedo = COMMON_MTL_TEXTURES[material.ALBEDO_TEX_IDX].Sample(smp, input.data.texCoord);

            // NOTE: It's need to be trasformed from sRGB space to linear
            // but since we copy RT0 to swapchain directly without any tone mapping etc. right now
            // and swapchain format is VK_FORMAT_R8G8B8A8_SRGB we don't need any transformations.
            // Will be fixed later
            //
            // result.color = float4(SRGBToLinear(albedo.rgb), albedo.a);

            result.color = albedo;
        }
    } else if (COMMON_DBG_IS_OUTPUT_COMMON_MTL_NORMAL_TEX) {
        if (material.NORMAL_TEX_IDX >= 0) {
            result.color = result.normal;
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