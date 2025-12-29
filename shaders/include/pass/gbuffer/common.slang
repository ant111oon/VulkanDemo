#ifndef GBUFFER_COMMON_H
#define GBUFFER_COMMON_H

#include "registers/common_registers.slang"

#include "common/common_defines.slang"
#include "common/math/color_utils.slang"


struct COMMON_SURFACE
{
    float4 albedo;
    float metalness;
    float3 emissive;
    float roughness;
    float3 lnorm;
    float ao;
};


float4 SampleCommonSurfaceAlbedo(in COMMON_MATERIAL mtl, in uint smpIdx, in float2 uv)
{
#ifdef ENV_DEBUG
    if (mtl.ALBEDO_TEX_IDX < 0) {
        return SAMPLE_ARR_TEX(COMMON_DBG_TEXTURES, uint(COMMON_DBG_TEX_IDX::CHECKERBOARD), COMMON_SAMPLERS[(uint)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_REPEAT], uv);
    }
#endif

    float4 albedo = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.ALBEDO_TEX_IDX, COMMON_SAMPLERS[smpIdx], uv);
    albedo.rgb = SRGBToLinear(albedo.rgb);

    albedo *= mtl.ALBEDO_MULT;

    return albedo;
}


float3 SampleCommonSurfaceEmissive(in COMMON_MATERIAL mtl, in uint smpIdx, in float2 uv)
{
#ifdef ENV_DEBUG
    if (mtl.EMISSIVE_TEX_IDX < 0) {
        return M3D_ZEROF3;
    }
#endif

    float3 emissive = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.EMISSIVE_TEX_IDX, COMMON_SAMPLERS[smpIdx], uv).rgb;
    emissive = SRGBToLinear(emissive);

    emissive *= mtl.EMISSIVE_MULT;

    return emissive;
}


float3 SampleCommonSurfaceNormal(in COMMON_MATERIAL mtl, in uint smpIdx, in float2 uv, in float3x3 TBN)
{
    float3 lnorm = M3D_AXIS_Z;

#ifdef ENV_DEBUG
    if (mtl.NORMAL_TEX_IDX >= 0)
#endif
    {
        lnorm = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.NORMAL_TEX_IDX, COMMON_SAMPLERS[smpIdx], uv).xyz;
        lnorm = normalize((lnorm * 2.f - 1.f) * float3(FLOAT2(mtl.NORMAL_SCALE), 1.f));
    }

    return normalize(mul(lnorm, TBN));
}


void SampleCommonSurfaceMetRoughAO(in COMMON_MATERIAL mtl, in uint smpIdx, in float2 uv, out float metalness, out float roughness, out float ao)
{
    float3 aoMetRough = float3(1.f, 0.f, 1.f);

    if (mtl.MR_TEX_IDX == mtl.AO_TEX_IDX) {
    #ifdef ENV_DEBUG
        if (mtl.MR_TEX_IDX >= 0)
    #endif
        {
            aoMetRough = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.MR_TEX_IDX, COMMON_SAMPLERS[smpIdx], uv).rgb;
        }
    } else {
    #ifdef ENV_DEBUG
        if (mtl.AO_TEX_IDX >= 0)
    #endif
        {
            aoMetRough.r = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.AO_TEX_IDX, COMMON_SAMPLERS[smpIdx], uv).r;
        }

    #ifdef ENV_DEBUG
        if (mtl.MR_TEX_IDX >= 0)
    #endif
        {
            aoMetRough.gb = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.MR_TEX_IDX, COMMON_SAMPLERS[smpIdx], uv).bg;
        }
    }

    metalness = aoMetRough.g * mtl.METALNESS_SCALE;
    roughness = aoMetRough.b * mtl.ROUGHNESS_SCALE;
    ao = lerp(1.f, aoMetRough.r, mtl.AO_COEF);
}


COMMON_SURFACE SampleCommonSurface(in uint mtlIdx, in float3x3 TBN, in float2 uv)
{
    COMMON_SURFACE surf = (COMMON_SURFACE)0;

    const COMMON_MATERIAL mtl = COMMON_MATERIALS[mtlIdx];

    const uint smpIdx = (uint)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT;

    surf.albedo = SampleCommonSurfaceAlbedo(mtl, smpIdx, uv);
    surf.emissive = SampleCommonSurfaceEmissive(mtl, smpIdx, uv);
    surf.lnorm = SampleCommonSurfaceNormal(mtl, smpIdx, uv, TBN);

    SampleCommonSurfaceMetRoughAO(mtl, smpIdx, uv, surf.metalness, surf.roughness, surf.ao);

    return surf;
}


#endif