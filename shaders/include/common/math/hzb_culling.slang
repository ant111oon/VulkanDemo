#ifndef MATH_HZB_CULLING_H
#define MATH_HZB_CULLING_H

#include "common/common_hzb.slang"


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

#include "geom_primitives.slang"


bool HZB_Visibility(in float2 minUV, in float2 maxUV, in float closestZ)
{
    if (any(minUV > maxUV)) {
        return true;
    }

    minUV = saturate(minUV);
    maxUV = saturate(maxUV);

    const float2 sizeInPixels = (maxUV - minUV) * (float2)HZB_CULLING_HZB_MIP_0_SIZE;

    if (any(sizeInPixels <= ZEROF2)) {
        return false;
    }

    float mip = ceil(log2(max(sizeInPixels.x, sizeInPixels.y)));
    mip = clamp(mip, 0, HZB_CULLING_HZB_MIPS_COUNT - 1);

    const float4 sbox = float4(minUV, maxUV);

    const float levelLower = max(mip - 1.f, 0.f);
    const float2 scale = exp2(-levelLower) * (float2)HZB_CULLING_HZB_MIP_0_SIZE;
    const float2 a = floor(sbox.xy * scale);
    const float2 b = ceil(sbox.zw * scale);
    const float2 dims = b - a;

    if (all(dims <= FLOAT2(2.f))) {
        mip = levelLower;
    }

    float4 depth = ZEROF4;
    depth.x = SAMPLE_TEX_LEVEL(HZB_CULLING_HZB_TEX, HZB_CULLING_HZB_TEX_SAMPLER, sbox.xy, mip).x;
    depth.y = SAMPLE_TEX_LEVEL(HZB_CULLING_HZB_TEX, HZB_CULLING_HZB_TEX_SAMPLER, sbox.zy, mip).x;
    depth.z = SAMPLE_TEX_LEVEL(HZB_CULLING_HZB_TEX, HZB_CULLING_HZB_TEX_SAMPLER, sbox.xw, mip).x;
    depth.w = SAMPLE_TEX_LEVEL(HZB_CULLING_HZB_TEX, HZB_CULLING_HZB_TEX_SAMPLER, sbox.zw, mip).x;

#ifdef ENV_REVERSED_Z
    const float hzbClosestZ = max(max(depth.x, depth.y), max(depth.z, depth.w));
    return closestZ >= hzbClosestZ;
#else
    const float hzbClosestZ = min(min(depth.x, depth.y), min(depth.z, depth.w));
    return closestZ <= hzbClosestZ;
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