#ifndef MATH_HZB_CULLING_H
#define MATH_HZB_CULLING_H

#ifndef HZB_CULLING_HZB_TEX
    #error Undefined HZB_CULLING_HZB_TEX. Must be defined before hzb_culling.slang include
#endif

#ifndef HZB_CULLING_HZB_TEX_SAMPLER
    #error Undefined HZB_CULLING_HZB_TEX_SAMPLER. Must be defined before hzb_culling.slang include
#endif

#ifndef HZB_CULLING_HZB_MIP_0_SIZE
    #error Undefined HZB_CULLING_HZB_MIP_0_SIZE. Must be defined before hzb_culling.slang include
#endif

#ifndef HZB_CULLING_HZB_MIPS_COUNT
    #error Undefined HZB_CULLING_HZB_MIPS_COUNT. Must be defined before hzb_culling.slang include
#endif

#ifndef HZB_CULLING_VIEW_PROJ_MATR
    #error Undefined HZB_CULLING_VIEW_PROJ_MATR. Must be defined before hzb_culling.slang include
#endif


#include "common/system/sys_constants.slang"

#include "common/common_hzb.slang"

#include "common/math/common.slang"
#include "common/math/geom_primitives.slang"


bool HZB_Visibility(in float2 minUV, in float2 maxUV, in float closestZ)
{
    if (any(minUV > maxUV)) {
        return true;
    }

    const float2 sizeInPixels = (maxUV - minUV) * (float2)HZB_CULLING_HZB_MIP_0_SIZE;

    const float maxSizeInPixels = max(sizeInPixels.x, sizeInPixels.y);
    float mip = clamp(floor(log2(maxSizeInPixels)), 0.f, HZB_CULLING_HZB_MIPS_COUNT - 1);

    const float levelLower = max(mip - 1.f, 0.f);
    const float2 scale = exp2(-levelLower) * (float2)HZB_CULLING_HZB_MIP_0_SIZE;
    const float2 a = floor(minUV * scale);
    const float2 b = ceil(maxUV * scale);
    const float2 dims = b - a;

    if (all(dims <= FLOAT2(2.f))) {
        mip = levelLower;
    }

    const float4 sbox = float4(minUV, maxUV);

    float4 samples = ZEROF4;
    samples.x = SAMPLE_TEX_LEVEL(HZB_CULLING_HZB_TEX, HZB_CULLING_HZB_TEX_SAMPLER, sbox.xy, mip).x;
    samples.y = SAMPLE_TEX_LEVEL(HZB_CULLING_HZB_TEX, HZB_CULLING_HZB_TEX_SAMPLER, sbox.zy, mip).x;
    samples.z = SAMPLE_TEX_LEVEL(HZB_CULLING_HZB_TEX, HZB_CULLING_HZB_TEX_SAMPLER, sbox.xw, mip).x;
    samples.w = SAMPLE_TEX_LEVEL(HZB_CULLING_HZB_TEX, HZB_CULLING_HZB_TEX_SAMPLER, sbox.zw, mip).x;

#ifdef ENV_REVERSED_Z
    const float depth = Min4(samples);
    return closestZ >= depth;
#else
    const float depth = Max4(samples);
    return closestZ <= depth;
#endif
}


bool HZB_Visibility(in AABB aabb)
{
    float2 minUV;
    float2 maxUV;
    float closestZ;

    aabb.GetMinMaxUVZ(HZB_CULLING_VIEW_PROJ_MATR, minUV, maxUV, closestZ);

    return HZB_Visibility(minUV, maxUV, closestZ);
}

#endif