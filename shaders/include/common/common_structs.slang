#ifndef COMMON_STRUCTS_H
#define COMMON_STRUCTS_H


#include "registers/common_registers.slang"

#include "common/resource_defines.slang"
#include "common/math/utils.slang"


struct COMMON_INDIRECT_DRAW_CMD
{
    // NOTE: Don't change order of this variables!!!
    uint INDEX_COUNT;
    uint INSTANCE_COUNT;
    uint FIRST_INDEX;
    int VERTEX_OFFSET;
    uint FIRST_INSTANCE;
};


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
    float4 albedo = ONEF4;

    if (mtl.ALBEDO_TEX_IDX < 0) {
        albedo = SAMPLE_ARR_TEX(COMMON_DBG_TEXTURES, uint(COMMON_DBG_TEX_IDX::CHECKERBOARD), GET_COMMON_SAMPLER(ANISO_2X_NEAREST_REPEAT), uv);
    } else {
        albedo = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.ALBEDO_TEX_IDX, COMMON_SAMPLERS[smpIdx], uv);
        albedo.rgb = SRGBToLinear(albedo.rgb);
    }

    albedo *= mtl.ALBEDO_MULT;

    return albedo;
}


float3 SampleCommonSurfaceEmissive(in COMMON_MATERIAL mtl, in uint smpIdx, in float2 uv)
{
    if (mtl.EMISSIVE_TEX_IDX < 0) {
        return ZEROF3;
    }

    float3 emissive = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.EMISSIVE_TEX_IDX, COMMON_SAMPLERS[smpIdx], uv).rgb;
    emissive = SRGBToLinear(emissive);

    emissive *= mtl.EMISSIVE_MULT;

    return emissive;
}


float3 SampleCommonSurfaceNormal(in COMMON_MATERIAL mtl, in uint smpIdx, in float2 uv, in float3x3 TBN)
{
    if (mtl.NORMAL_TEX_IDX < 0) {
        return TBN[2];
    }

    float3 lnorm = 2.f * SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.NORMAL_TEX_IDX, COMMON_SAMPLERS[smpIdx], uv).xyz - 1.f;
    lnorm = normalize(lnorm * float3(FLOAT2(mtl.NORMAL_SCALE), 1.f));

    return normalize(mul(lnorm, TBN));
}


void SampleCommonSurfaceMetRoughAO(in COMMON_MATERIAL mtl, in uint smpIdx, in float2 uv, out float metalness, out float roughness, out float ao)
{
    float3 aoMetRough = float3(1.f, 1.f, 1.f);

    if (mtl.MR_TEX_IDX == mtl.AO_TEX_IDX) {
        if (mtl.MR_TEX_IDX >= 0) {
            aoMetRough = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.MR_TEX_IDX, COMMON_SAMPLERS[smpIdx], uv).rgb;
        }
    } else {
        if (mtl.AO_TEX_IDX >= 0) {
            aoMetRough.r = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.AO_TEX_IDX, COMMON_SAMPLERS[smpIdx], uv).r;
        }

        if (mtl.MR_TEX_IDX >= 0) {
            aoMetRough.gb = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.MR_TEX_IDX, COMMON_SAMPLERS[smpIdx], uv).bg;
        }
    }

    metalness = aoMetRough.g * mtl.METALNESS_SCALE;
    roughness = aoMetRough.b * mtl.ROUGHNESS_SCALE;
    ao = lerp(1.f, aoMetRough.r, mtl.AO_COEF);
}


COMMON_SURFACE SampleCommonSurface(in COMMON_MATERIAL material, in float3x3 TBN, in float2 uv)
{
    COMMON_SURFACE surf = (COMMON_SURFACE)0;

    const uint smpIdx = (uint)COMMON_SAMPLER_IDX::ANISO_16X_LINEAR_REPEAT;

    surf.albedo = SampleCommonSurfaceAlbedo(material, smpIdx, uv);
    surf.emissive = SampleCommonSurfaceEmissive(material, smpIdx, uv);
    surf.lnorm = SampleCommonSurfaceNormal(material, smpIdx, uv, TBN);

    SampleCommonSurfaceMetRoughAO(material, smpIdx, uv, surf.metalness, surf.roughness, surf.ao);

    return surf;
}


#endif