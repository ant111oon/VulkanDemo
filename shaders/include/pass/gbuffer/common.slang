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
    float metalness;
    float roughness;
    float ao;
};


COMMON_SURFACE SampleCommonSurface(in uint mtlIdx, in float3x3 TBN, in float2 uv)
{
    COMMON_SURFACE surf = (COMMON_SURFACE)0;

    const COMMON_MATERIAL mtl = COMMON_MATERIALS[mtlIdx];
    const SamplerState smp = COMMON_SAMPLERS[(uint)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT];

    if (mtl.ALBEDO_TEX_IDX >= 0) {
        surf.albedo = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.ALBEDO_TEX_IDX, smp, uv);
        surf.albedo = float4(SRGBToLinear(surf.albedo.rgb), surf.albedo.a);
    }

    if (mtl.EMISSIVE_TEX_IDX >= 0) {
        surf.emissive = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.EMISSIVE_TEX_IDX, smp, uv);
        surf.emissive = float4(SRGBToLinear(surf.emissive.rgb), surf.emissive.a);
    }

    if (mtl.NORMAL_TEX_IDX >= 0) {
        surf.lnorm = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.NORMAL_TEX_IDX, smp, uv).xyz;
        surf.lnorm = normalize(mul(normalize(surf.lnorm), TBN));
    }

    float3 aoMetRough = M3D_ZEROF3;
    if (mtl.MR_TEX_IDX == mtl.AO_TEX_IDX) {
        if (mtl.MR_TEX_IDX >= 0) {
            aoMetRough = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.MR_TEX_IDX, smp, uv).rgb;
        }
    } else {
        if (mtl.AO_TEX_IDX >= 0) {
            aoMetRough.r = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.AO_TEX_IDX, smp, uv).r;
        }

        if (mtl.MR_TEX_IDX >= 0) {
            aoMetRough.gb = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.MR_TEX_IDX, smp, uv).bg;
        }
    }

    surf.metalness = aoMetRough.g;
    surf.roughness = aoMetRough.b;
    surf.ao = aoMetRough.r;

    return surf;
}


#endif