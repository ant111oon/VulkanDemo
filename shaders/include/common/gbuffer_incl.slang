#ifndef GBUFFER_INCL_H
#define GBUFFER_INCL_H

#include "registers/common_registers.slang"

#include "common/math/color_space.slang"


struct GBUFFER_SURFACE
{
    float4 albedo;
    float metalness;
    float3 emissive;
    float roughness;
    float3 lnorm;
    float ao;
};


float4 SampleGBufferSurfaceAlbedo(in GPU_GeomMaterial mtl, in GPU_CommonSamplerID smpIdx, in float2 uv)
{
    if (mtl.albedoTexID < 0) {
        return ONEF4;
    }

    float4 albedo = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.albedoTexID, GET_COMMON_SAMPLER(smpIdx), uv);
    albedo.rgb = SRGBToLinear(albedo.rgb);

    albedo *= mtl.albedoMult;

    return albedo;
}


float3 SampleGBufferSurfaceEmissive(in GPU_GeomMaterial mtl, in GPU_CommonSamplerID smpIdx, in float2 uv)
{
    if (mtl.emissiveTexID < 0) {
        return ZEROF3;
    }

    float3 emissive = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.emissiveTexID, GET_COMMON_SAMPLER(smpIdx), uv).rgb;
    emissive = SRGBToLinear(emissive);

    emissive *= mtl.emissiveMult;

    return emissive;
}


float3 SampleGBufferSurfaceNormal(in GPU_GeomMaterial mtl, in GPU_CommonSamplerID smpIdx, in float2 uv, in float3x3 TBN)
{
    if (mtl.normalTexID < 0) {
        return TBN[2];
    }

    float3 lnorm = 2.f * SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.normalTexID, GET_COMMON_SAMPLER(smpIdx), uv).xyz - 1.f;
    lnorm = normalize(lnorm * float3(FLOAT2(mtl.normalScale), 1.f));

    return normalize(mul(lnorm, TBN));
}


void SampleGBufferSurfaceMetRoughAO(in GPU_GeomMaterial mtl, in GPU_CommonSamplerID smpIdx, in float2 uv, out float metalness, out float roughness, out float ao)
{
    float3 aoMetRough = float3(1.f, 1.f, 1.f);

    if (mtl.metalRoughTexID == mtl.aoTexID) {
        if (mtl.metalRoughTexID >= 0) {
            aoMetRough = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.metalRoughTexID, GET_COMMON_SAMPLER(smpIdx), uv).rgb;
        }
    } else {
        if (mtl.aoTexID >= 0) {
            aoMetRough.r = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.aoTexID, GET_COMMON_SAMPLER(smpIdx), uv).r;
        }

        if (mtl.metalRoughTexID >= 0) {
            aoMetRough.gb = SAMPLE_ARR_TEX(COMMON_MTL_TEXTURES, mtl.metalRoughTexID, GET_COMMON_SAMPLER(smpIdx), uv).bg;
        }
    }

    metalness = aoMetRough.g * mtl.metalnessScale;
    roughness = aoMetRough.b * mtl.roughnessScale;
    ao = lerp(1.f, aoMetRough.r, mtl.aoCoef);
}


GBUFFER_SURFACE SampleGBufferSurface(in GPU_GeomMaterial material, in float3x3 TBN, in float2 uv)
{
    GBUFFER_SURFACE surf = (GBUFFER_SURFACE)0;

    const GPU_CommonSamplerID smpIdx = GPU_CommonSamplerID::ANISO_2X_LINEAR_REPEAT;

    surf.albedo = SampleGBufferSurfaceAlbedo(material, smpIdx, uv);
    surf.emissive = SampleGBufferSurfaceEmissive(material, smpIdx, uv);
    surf.lnorm = SampleGBufferSurfaceNormal(material, smpIdx, uv, TBN);

    SampleGBufferSurfaceMetRoughAO(material, smpIdx, uv, surf.metalness, surf.roughness, surf.ao);

    return surf;
}


#endif