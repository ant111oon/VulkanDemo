#ifndef MATH_HZB_CULLING_H
#define MATH_HZB_CULLING_H


#ifndef HZB_CULLING_HZB_MIP_0_SIZE
    #error Undefined HZB_CULLING_HZB_MIP_0_SIZE. Must be defined before hzb_culling.slang include
#endif

#ifndef HZB_CULLING_HZB_MIPS_COUNT
    #error Undefined HZB_CULLING_HZB_MIPS_COUNT. Must be defined before hzb_culling.slang include
#endif

#ifndef HZB_CULLING_VIEW_PROJ_MATR_PREV
    #error Undefined HZB_CULLING_VIEW_PROJ_MATR_PREV. Must be defined before hzb_culling.slang include
#endif

#ifndef HZB_CULLING_VIEW_NEAR_PLANE
    #error Undefined HZB_CULLING_VIEW_NEAR_PLANE. Must be defined before hzb_culling.slang include
#endif


#include "common/system/sys_constants.slang"

#include "common/math/common.slang"
#include "common/math/geom_primitives.slang"


bool HZB_Visibility(in GPU_SBox sbox, Texture2D HZB)
{
    if (any(sbox.minUV >= sbox.maxUV)) {
        return true;
    }

    const float2 sizeInPixels = sbox.GetDeltaUV() * (float2)HZB_CULLING_HZB_MIP_0_SIZE;

    float mip = clamp(ceil(log2(max(sizeInPixels.x, sizeInPixels.y))), 0.f, HZB_CULLING_HZB_MIPS_COUNT - 1);

    const float levelLower = max(mip - 1.f, 0.f);
    const float2 scale = exp2(-levelLower) * (float2)HZB_CULLING_HZB_MIP_0_SIZE;
    const float2 a = floor(sbox.minUV * scale);
    const float2 b = ceil(sbox.maxUV * scale);
    const float2 dims = b - a;

    if (all(dims <= FLOAT2(2.f))) {
        mip = levelLower;
    }

    SamplerState hzbSampler = GET_COMMON_SMP(GPU_SamplerID::NEAREST_CLAMP_TO_EDGE);

    float4 samples = ZEROF4;
    samples.x = SAMPLE_TEX_LEVEL(HZB, hzbSampler, sbox.GetCornerBL(), mip).x;
    samples.y = SAMPLE_TEX_LEVEL(HZB, hzbSampler, sbox.GetCornerBR(), mip).x;
    samples.z = SAMPLE_TEX_LEVEL(HZB, hzbSampler, sbox.GetCornerUL(), mip).x;
    samples.w = SAMPLE_TEX_LEVEL(HZB, hzbSampler, sbox.GetCornerUR(), mip).x;

#ifdef ENV_REVERSED_Z
    const float depth = Min(samples);
    return sbox.closestZ >= depth;
#else
    const float depth = Max(samples);
    return sbox.closestZ <= depth;
#endif
}


bool HZB_Visibility(in GPU_AABB aabb, Texture2D HZB)
{
    if (aabb.IntersectsPlane(HZB_CULLING_VIEW_NEAR_PLANE)) {
        return true;
    }

    GPU_SBox sbox = aabb.GetSBox(HZB_CULLING_VIEW_PROJ_MATR_PREV);
    sbox.Saturate();

    return HZB_Visibility(sbox, HZB);
}

#endif