#include "registers/common_registers.slang"
#include "math/color_utils.slang"

#include "pass/gbuffer/vsout.slang"


struct GBUFFER_PS_OUTPUT
{
    [[vk::location(0)]] float4 color;
    [[vk::location(1)]] float4 normal;
};


[shader("fragment")]
GBUFFER_PS_OUTPUT main(GBUFFER_VS_OUTPUT input)
{
    GBUFFER_PS_OUTPUT result = (GBUFFER_PS_OUTPUT)0;

    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[input.flatData.instID];
    const COMMON_MATERIAL material = COMMON_MATERIALS[instInfo.MATERIAL_IDX];

    const SamplerState smp = COMMON_SAMPLERS[(uint)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT];

    const float4 albedo = material.ALBEDO_TEX_IDX >= 0 ? COMMON_MTL_TEXTURES[material.ALBEDO_TEX_IDX].Sample(smp, input.data.texCoord) : M3D_ZEROF4;
    const float3 normal = material.NORMAL_TEX_IDX >= 0 ? COMMON_MTL_TEXTURES[material.NORMAL_TEX_IDX].Sample(smp, input.data.texCoord).xyz : M3D_ZEROF3;
    const float4 emissive = material.EMISSIVE_TEX_IDX >= 0 ? COMMON_MTL_TEXTURES[material.EMISSIVE_TEX_IDX].Sample(smp, input.data.texCoord) : M3D_ZEROF4;

    const bool isMergedAoMetRough = material.MR_TEX_IDX == material.AO_TEX_IDX;

    float3 aoMetRough = M3D_ZEROF3;
    if (isMergedAoMetRough) {
        aoMetRough = material.MR_TEX_IDX >= 0 ? COMMON_MTL_TEXTURES[material.MR_TEX_IDX].Sample(smp, input.data.texCoord).rgb : M3D_ZEROF3;
    } else {
        aoMetRough.r = material.AO_TEX_IDX >= 0 ? COMMON_MTL_TEXTURES[material.AO_TEX_IDX].Sample(smp, input.data.texCoord).r : 0.f;
        aoMetRough.gb = material.MR_TEX_IDX >= 0 ? COMMON_MTL_TEXTURES[material.MR_TEX_IDX].Sample(smp, input.data.texCoord).bg : M3D_ZEROF2;
    }

    result.normal = float4(M3D_IS_ZERO(normal) ? normal : normalize(normal), 1.f);

    if (COMMON_DBG_IS_OUTPUT_COMMON_MTL_ALBEDO_TEX) {
        // NOTE: It's need to be trasformed from sRGB space to linear
        // but since we copy RT0 to swapchain directly without any tone mapping etc. right now
        // and swapchain format is VK_FORMAT_R8G8B8A8_SRGB we don't need any transformations.
        // Will be fixed later
        //
        // result.color = float4(SRGBToLinear(albedo.rgb), albedo.a);

        result.color = albedo;
    } else if (COMMON_DBG_IS_OUTPUT_COMMON_MTL_NORMAL_TEX) {
        result.color = result.normal;
    } else if (COMMON_DBG_IS_OUTPUT_COMMON_MTL_MR_TEX) {
        result.color = float4(aoMetRough.gb, 0.f, 1.f);
    } else if (COMMON_DBG_IS_OUTPUT_COMMON_MTL_AO_TEX) {
        result.color = float4(aoMetRough.rrr, 1.f);
    } else if (COMMON_DBG_IS_OUTPUT_COMMON_MTL_EMISSIVE_TEX) {
        result.color = emissive;
    }

    if (result.color.a <= 0.0001f) {
        discard;
    }
    
    return result;
}