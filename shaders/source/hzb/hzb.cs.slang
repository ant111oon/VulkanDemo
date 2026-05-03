#include "registers/common_registers.slang"
#include "registers/hzb_registers.slang"

#include "common/common_cs.slang"
#include "common/math/common.slang"


#if defined(ENV_REVERSED_Z)
    #define HZB_GET_FAREST_DEPTH(VAL0, ...) Min(VAL0, __VA_ARGS__)
#else
    #define HZB_GET_FAREST_DEPTH(VAL0, ...) Max(VAL0, __VA_ARGS__)
#endif


[shader("compute")]
[numthreads(HZB_BUILD_CS_GROUP_SIZE, HZB_BUILD_CS_GROUP_SIZE, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    const uint2 dstMipSize = max(HZB_GEN_DST_MIP_RESOLUTION, ONEU2);

    if (any(DTid >= dstMipSize)) {
        return;
    }

    const uint mip = HZB_GEN_DST_MIP_IDX;

    if (HZB_GEN_IS_DEPTH_BUFFER_COPY_PASS) {
        HZB_DST_MIP_UAVS[mip][DTid] = LOAD_TEX_ARRAY(HZB_SRC_MIPS, mip, int3(DTid, 0));
        return;
    }

    const uint prevMip = mip - 1u;
    const uint2 srcMipSize = HZB_GEN_SRC_MIP_RESOLUTION;

    const int2 dstPixCoord = DTid;
    const int2 srcPixCoord = min(dstPixCoord * 2, srcMipSize - 1);

    const int3 texCoord0 = int3(srcPixCoord, 0);
    const int3 texCoord1 = int3(min(srcPixCoord + int2(1, 0), srcMipSize - 1), 0);
    const int3 texCoord2 = int3(min(srcPixCoord + int2(0, 1), srcMipSize - 1), 0);
    const int3 texCoord3 = int3(min(srcPixCoord + int2(1, 1), srcMipSize - 1), 0);

    float4 samples;
    samples.x = LOAD_TEX_ARRAY(HZB_SRC_MIPS, prevMip, texCoord0).r;
    samples.y = LOAD_TEX_ARRAY(HZB_SRC_MIPS, prevMip, texCoord1).r;
    samples.z = LOAD_TEX_ARRAY(HZB_SRC_MIPS, prevMip, texCoord2).r;
    samples.w = LOAD_TEX_ARRAY(HZB_SRC_MIPS, prevMip, texCoord3).r;

    float depth = HZB_GET_FAREST_DEPTH(samples);

    // Incorporate additional texels if the previous level's width or height (or both) are odd.
    const bool isPrevMipWidthOdd = IS_FLAG_SET(srcMipSize.x, 1u);
    const bool isPrevMipHeightOdd = IS_FLAG_SET(srcMipSize.y, 1u);

    if (isPrevMipWidthOdd) {
        const int3 texCoord4 = int3(min(srcPixCoord + int2(2, 0), srcMipSize - 1), 0);
        const int3 texCoord5 = int3(min(srcPixCoord + int2(2, 1), srcMipSize - 1), 0);

        const float2 extraValues = float2(
            LOAD_TEX_ARRAY(HZB_SRC_MIPS, prevMip, texCoord4).r,
            LOAD_TEX_ARRAY(HZB_SRC_MIPS, prevMip, texCoord5).r
        );

        depth = HZB_GET_FAREST_DEPTH(depth, HZB_GET_FAREST_DEPTH(extraValues));
    }

    if (isPrevMipHeightOdd) {
        const int3 texCoord4 = int3(min(srcPixCoord + int2(0, 2), srcMipSize - 1), 0);
        const int3 texCoord5 = int3(min(srcPixCoord + int2(1, 2), srcMipSize - 1), 0);

        const float2 extraValues = float2(
            LOAD_TEX_ARRAY(HZB_SRC_MIPS, prevMip, texCoord4).r,
            LOAD_TEX_ARRAY(HZB_SRC_MIPS, prevMip, texCoord5).r
        );

        depth = HZB_GET_FAREST_DEPTH(depth, HZB_GET_FAREST_DEPTH(extraValues));
    }

    if (isPrevMipWidthOdd && isPrevMipHeightOdd) {
        const int3 texCoord6 = int3(min(srcPixCoord + int2(2, 2), srcMipSize - 1), 0);

        const float extraCornerValue = LOAD_TEX_ARRAY(HZB_SRC_MIPS, prevMip, texCoord6).r;

        depth = HZB_GET_FAREST_DEPTH(depth, extraCornerValue);
    }

    HZB_DST_MIP_UAVS[mip][dstPixCoord] = depth;
}
