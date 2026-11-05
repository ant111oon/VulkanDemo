#ifndef COMMON_GEOM_H
#define COMMON_GEOM_H

#include "registers/common_registers.slang"

#include "common/math/color_space.slang"


struct GPU_SampledGeomMaterial
{
    float4 albedo;
    float metalness;
    float3 emissive;
    float roughness;
    float3 lnorm;
    float ao;
};


float4 SampleGeomMaterialAlbedo(in GPU_GeomMaterial mtl, in float2 uv)
{
    if (!mtl.albedoTex.IsValid()) {
        return ONEF4;
    }

    float4 albedo = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.albedoTex.ID, GET_COMMON_MTL_SMP(mtl.albedoTex.samplerID), uv);
    albedo *= mtl.albedoMult;

    return albedo;
}


float3 SampleGeomMaterialEmissive(in GPU_GeomMaterial mtl, in float2 uv)
{
    if (!mtl.emissiveTex.IsValid()) {
        return ZEROF3;
    }

    float3 emissive = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.emissiveTex.ID, GET_COMMON_MTL_SMP(mtl.emissiveTex.samplerID), uv).rgb;
    emissive *= mtl.emissiveMult;

    return emissive;
}


float3 SampleGeomMaterialNormal(in GPU_GeomMaterial mtl, in float2 uv, in float3x3 TBN)
{
    if (!mtl.normalTex.IsValid()) {
        return TBN[2];
    }

    float3 lnorm = 2.f * SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.normalTex.ID, GET_COMMON_MTL_SMP(mtl.normalTex.samplerID), uv).xyz - 1.f;
    lnorm = normalize(lnorm * float3(FLOAT2(mtl.normalScale), 1.f));

    return normalize(mul(lnorm, TBN));
}


float3 SampleGeomMaterialMetRoughAO(in GPU_GeomMaterial mtl, in float2 uv)
{
    float3 aoMetRough = ONEF3;

    if (mtl.metalRoughTex == mtl.aoTex) {
        if (mtl.metalRoughTex.IsValid()) {
            aoMetRough = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.metalRoughTex.ID, GET_COMMON_MTL_SMP(mtl.metalRoughTex.samplerID), uv).rgb;
        }
    } else {
        if (mtl.aoTex.IsValid()) {
            aoMetRough.r = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.aoTex.ID, GET_COMMON_MTL_SMP(mtl.aoTex.samplerID), uv).r;
        }

        if (mtl.metalRoughTex.IsValid()) {
            aoMetRough.gb = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.metalRoughTex.ID, GET_COMMON_MTL_SMP(mtl.metalRoughTex.samplerID), uv).bg;
        }
    }

    float3 result;
    result.r = aoMetRough.g * mtl.metalnessScale;
    result.g = aoMetRough.b * mtl.roughnessScale;
    result.b = lerp(1.f, aoMetRough.r, mtl.aoCoef);

    return result;
}


GPU_SampledGeomMaterial SampleGeomMaterial(in GPU_GeomMaterial template, in float3x3 TBN, in float2 uv)
{
    GPU_SampledGeomMaterial mtl = (GPU_SampledGeomMaterial)0;

    mtl.albedo = SampleGeomMaterialAlbedo(template, uv);
    mtl.emissive = SampleGeomMaterialEmissive(template, uv);
    mtl.lnorm = SampleGeomMaterialNormal(template, uv, TBN);

    const float3 aoMetRough = SampleGeomMaterialMetRoughAO(template, uv);
    mtl.metalness = aoMetRough.r;
    mtl.roughness = aoMetRough.g;
    mtl.ao = aoMetRough.b;

    return mtl;
}


#endif