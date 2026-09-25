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
    if (mtl.albedoTexID < 0) {
        return ONEF4;
    }

    float4 albedo = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.albedoTexID, GET_COMMON_SMP(GPU_SamplerID::ANISO_2X_LINEAR_REPEAT), uv);
    albedo.rgb = SRGBToLinear(albedo.rgb);

    albedo *= mtl.albedoMult;

    return albedo;
}


float3 SampleGeomMaterialEmissive(in GPU_GeomMaterial mtl, in float2 uv)
{
    if (mtl.emissiveTexID < 0) {
        return ZEROF3;
    }

    float3 emissive = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.emissiveTexID, GET_COMMON_SMP(GPU_SamplerID::ANISO_2X_LINEAR_REPEAT), uv).rgb;
    emissive = SRGBToLinear(emissive);

    emissive *= mtl.emissiveMult;

    return emissive;
}


float3 SampleGeomMaterialNormal(in GPU_GeomMaterial mtl, in float2 uv, in float3x3 TBN)
{
    if (mtl.normalTexID < 0) {
        return TBN[2];
    }

    float3 lnorm = 2.f * SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.normalTexID, GET_COMMON_SMP(GPU_SamplerID::ANISO_2X_LINEAR_REPEAT), uv).xyz - 1.f;
    lnorm = normalize(lnorm * float3(FLOAT2(mtl.normalScale), 1.f));

    return normalize(mul(lnorm, TBN));
}


float3 SampleGeomMaterialMetRoughAO(in GPU_GeomMaterial mtl, in float2 uv)
{
    float3 aoMetRough = ONEF3;

    SamplerState smp = GET_COMMON_SMP(GPU_SamplerID::ANISO_2X_LINEAR_REPEAT);

    if (mtl.metalRoughTexID == mtl.aoTexID) {
        if (mtl.metalRoughTexID >= 0) {
            aoMetRough = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.metalRoughTexID, smp, uv).rgb;
        }
    } else {
        if (mtl.aoTexID >= 0) {
            aoMetRough.r = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.aoTexID, smp, uv).r;
        }

        if (mtl.metalRoughTexID >= 0) {
            aoMetRough.gb = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.metalRoughTexID, smp, uv).bg;
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