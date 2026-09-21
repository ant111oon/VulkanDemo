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

    if (all(srcMipSize == dstMipSize)) {
        HZB_DST_MIP_UAV[dstPixCoord] = LOAD_TEX(HZB_SRC_MIP, int3(dstPixCoord, 0));
        return;
    }

    const float2 uv = ((float2)dstPixCoord + 0.5f) / (float2)dstMipSize;

    const float4 samples = GATHER_RED_TEX(HZB_SRC_MIP, GET_COMMON_SMP(GPU_SamplerID::NEAREST_CLAMP_TO_EDGE), uv);
    const float depth = HZB_GET_FAREST_DEPTH(samples);

    HZB_DST_MIP_UAV[dstPixCoord] = depth;
}
