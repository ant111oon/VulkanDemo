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


float4 SampleCommonSurfaceAlbedo(in COMMON_MATERIAL mtl, in SamplerState sampler, in float2 uv)
{
#ifdef ENV_DEBUG
    if (mtl.ALBEDO_TEX_IDX < 0) {
        return SAMPLE_ARR_TEX(COMMON_DBG_TEXTURES, uint(COMMON_DBG_TEX_IDX::CHECKERBOARD), COMMON_SAMPLERS[(uint)COMMON_SAMPLER_IDX::ANISO_2X_NEAREST_REPEAT], uv);
    }
#endif

    float4 albedo = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.ALBEDO_TEX_IDX, sampler, uv);
    albedo.rgb = SRGBToLinear(albedo.rgb);

    return albedo;
}


float4 SampleCommonSurfaceEmissive(in COMMON_MATERIAL mtl, in SamplerState sampler, in float2 uv)
{
#ifdef ENV_DEBUG
    if (mtl.EMISSIVE_TEX_IDX < 0) {
        return float4(0.f, 0.f, 0.f, 1.f);
    }
#endif

    float4 emissive = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.EMISSIVE_TEX_IDX, sampler, uv);
    emissive.rgb = SRGBToLinear(emissive.rgb);

    return emissive;
}


float3 SampleCommonSurfaceNormal(in COMMON_MATERIAL mtl, in SamplerState sampler, in float2 uv, in float3x3 TBN)
{
    float3 lnorm = M3D_AXIS_Z;

#ifdef ENV_DEBUG
    if (mtl.NORMAL_TEX_IDX >= 0)
#endif
    {
        lnorm = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.NORMAL_TEX_IDX, sampler, uv).xyz;
    }

    return normalize(mul(normalize(lnorm), TBN));
}


float3 SampleCommonSurfaceMetRoughAO(in COMMON_MATERIAL mtl, in SamplerState sampler, in float2 uv)
{
    float3 aoMetRough = float3(1.f, 0.f, 1.f);

    if (mtl.MR_TEX_IDX == mtl.AO_TEX_IDX) {
        #ifdef ENV_DEBUG
        if (mtl.MR_TEX_IDX >= 0)
        #endif
        {
            aoMetRough = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.MR_TEX_IDX, sampler, uv).rgb;
        }
    } else {
        #ifdef ENV_DEBUG
        if (mtl.AO_TEX_IDX >= 0)
        #endif
        {
            aoMetRough.r = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.AO_TEX_IDX, sampler, uv).r;
        }

        #ifdef ENV_DEBUG
        if (mtl.MR_TEX_IDX >= 0)
        #endif
        {
            aoMetRough.gb = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.MR_TEX_IDX, sampler, uv).bg;
        }
    }

    return float3(aoMetRough.g, aoMetRough.b, aoMetRough.r);
}


COMMON_SURFACE SampleCommonSurface(in uint mtlIdx, in float3x3 TBN, in float2 uv)
{
    COMMON_SURFACE surf = (COMMON_SURFACE)0;

    const COMMON_MATERIAL mtl = COMMON_MATERIALS[mtlIdx];
    const SamplerState sampler = COMMON_SAMPLERS[(uint)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT];

    surf.albedo = SampleCommonSurfaceAlbedo(mtl, sampler, uv);
    surf.emissive = SampleCommonSurfaceEmissive(mtl, sampler, uv);
    surf.lnorm = SampleCommonSurfaceNormal(mtl, sampler, uv, TBN);

    const float3 metRoughAO = SampleCommonSurfaceMetRoughAO(mtl, sampler, uv);

    surf.metalness = metRoughAO.r;
    surf.roughness = metRoughAO.g;
    surf.ao = metRoughAO.b;

    return surf;
}


#endif