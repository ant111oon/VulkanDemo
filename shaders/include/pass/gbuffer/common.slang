#ifndef GBUFFER_COMMON_H
#define GBUFFER_COMMON_H

#include "registers/common_registers.slang"
#include "common/common_defines.slang"
#include "math/color_utils.slang"


struct COMMON_SURFACE
{
    float4 albedo;
    float4 emissive;
    float3 lnorm;
    float3 wnorm;
    float metalness;
    float roughness;
    float ao;
};


COMMON_SURFACE SampleCommonSurface(in uint mtlIdx, in float3x3 TBN, in float3x4 wMatr, in float2 uv)
{
    COMMON_SURFACE surf = (COMMON_SURFACE)0;

    const COMMON_MATERIAL mtl = COMMON_MATERIALS[mtlIdx];
    const SamplerState smp = COMMON_SAMPLERS[(uint)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT];

    surf.albedo = mtl.ALBEDO_TEX_IDX >= 0 ? SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.ALBEDO_TEX_IDX, smp, uv) : M3D_ZEROF4;
    surf.albedo = float4(SRGBToLinear(surf.albedo.rgb), surf.albedo.a);

    surf.emissive = mtl.EMISSIVE_TEX_IDX >= 0 ? SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.EMISSIVE_TEX_IDX, smp, uv) : M3D_ZEROF4;
    surf.emissive = float4(SRGBToLinear(surf.emissive.rgb), surf.emissive.a);

    surf.lnorm = mtl.NORMAL_TEX_IDX >= 0 ? SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.NORMAL_TEX_IDX, smp, uv).xyz : M3D_ZEROF3;
    surf.lnorm = M3D_IS_ZERO(surf.lnorm) ? M3D_ZEROF3 : normalize(mul(surf.lnorm, TBN));

    surf.wnorm = M3D_IS_ZERO(surf.lnorm) ? M3D_ZEROF3 : normalize(mul(wMatr, float4(surf.lnorm, 0.f)));

    float3 aoMetRough = M3D_ZEROF3;
    if (mtl.MR_TEX_IDX == mtl.AO_TEX_IDX) {
        aoMetRough = mtl.MR_TEX_IDX >= 0 ? SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.MR_TEX_IDX, smp, uv).rgb : M3D_ZEROF3;
    } else {
        aoMetRough.r = mtl.AO_TEX_IDX >= 0 ? SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.AO_TEX_IDX, smp, uv).r : 0.f;
        aoMetRough.gb = mtl.MR_TEX_IDX >= 0 ? SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.MR_TEX_IDX, smp, uv).bg : M3D_ZEROF2;
    }

    surf.metalness = aoMetRough.g;
    surf.roughness = aoMetRough.b;
    surf.ao = aoMetRough.r;

    return surf;
}


#endif