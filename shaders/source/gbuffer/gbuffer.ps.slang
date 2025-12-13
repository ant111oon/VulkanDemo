#include "registers/common_registers.slang"
#include "math/color_utils.slang"

#include "common/dbg_vis.slang"
#include "common/common_defines.slang"

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
    const COMMON_MATERIAL mtl = COMMON_MATERIALS[instInfo.MATERIAL_IDX];

    const SamplerState smp = COMMON_SAMPLERS[(uint)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT];

    // NOTE: It's need to be trasformed from sRGB space to linear
    // but since we copy RT0 to swapchain directly without any tone mapping etc. right now
    // and swapchain format is VK_FORMAT_R8G8B8A8_SRGB we don't need any transformations.
    // Will be fixed later
    //
    // albedo = float4(SRGBToLinear(albedo.rgb), albedo.a);
    // emissive = float4(SRGBToLinear(emissive.rgb), emissive.a);
    const float4 albedo = mtl.ALBEDO_TEX_IDX >= 0 ? SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.ALBEDO_TEX_IDX, smp, input.data.texCoord) : M3D_ZEROF4;
    const float4 emissive = mtl.EMISSIVE_TEX_IDX >= 0 ? SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.EMISSIVE_TEX_IDX, smp, input.data.texCoord) : M3D_ZEROF4;

    const float3 normal = mtl.NORMAL_TEX_IDX >= 0 ? SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.NORMAL_TEX_IDX, smp, input.data.texCoord).xyz : M3D_ZEROF3;

    const bool isMergedAoMetRough = mtl.MR_TEX_IDX == mtl.AO_TEX_IDX;

    float3 aoMetRough = M3D_ZEROF3;
    if (isMergedAoMetRough) {
        aoMetRough = mtl.MR_TEX_IDX >= 0 ? SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.MR_TEX_IDX, smp, input.data.texCoord).rgb : M3D_ZEROF3;
    } else {
        aoMetRough.r = mtl.AO_TEX_IDX >= 0 ? SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.AO_TEX_IDX, smp, input.data.texCoord).r : 0.f;
        aoMetRough.gb = mtl.MR_TEX_IDX >= 0 ? SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.MR_TEX_IDX, smp, input.data.texCoord).bg : M3D_ZEROF2;
    }

    if (albedo.a <= 0.0001f) {
        discard;
    }

    result.color = albedo;
    result.normal = float4(M3D_IS_ZERO(normal) ? normal : normalize(normal), 1.f);

    DbgVisOutput(result.color, albedo, normal, aoMetRough.g, aoMetRough.b, aoMetRough.r, emissive);

    return result;
}