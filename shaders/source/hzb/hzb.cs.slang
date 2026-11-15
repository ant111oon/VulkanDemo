#include "registers/common_registers.slang"
#include "registers/hzb_registers.slang"

#include "common/math/common.slang"


#if defined(ENV_REVERSED_Z)
    #define HZB_GET_FAREST_DEPTH(VAL0, ...) Min(VAL0, __VA_ARGS__)
#else
    #define HZB_GET_FAREST_DEPTH(VAL0, ...) Max(VAL0, __VA_ARGS__)
#endif


[shader("compute")]
[numthreads(HZB_BUILD_CS_GROUP_SIZE, HZB_BUILD_CS_GROUP_SIZE, 1)]
void main(SV_GROUP_THREAD_ID(groupThreadID), SV_DISPATCH_THREAD_ID(dispatchThreadID))
{
    const uint2 srcMipSize = max(HZB_GEN_SRC_MIP_RESOLUTION, ONEU2);
    const uint2 dstMipSize = max(HZB_GEN_DST_MIP_RESOLUTION, ONEU2);

    const uint2 dstPixCoord = dispatchThreadID.xy;

    if (any(dstPixCoord >= dstMipSize)) {
        return;
    }

    const uint2 srcBegin = (dstPixCoord * srcMipSize) / dstMipSize;
    const uint2 srcEnd   = min(((dstPixCoord + ONEU2) * srcMipSize + dstMipSize - ONEU2) / dstMipSize, srcMipSize);

#if defined(ENV_REVERSED_Z)
    float depth = 1.f;
#else
    float depth = 0.f;
#endif

    for (uint y = srcBegin.y; y < srcEnd.y; ++y) {
        for (uint x = srcBegin.x; x < srcEnd.x; ++x) {
            depth = HZB_GET_FAREST_DEPTH(depth, LOAD_TEX(HZB_SRC_MIP, int3(x, y, 0)).r);
        }
    }

    HZB_DST_MIP_UAV[dstPixCoord] = depth;
}
