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
void main(uint2 DTid : SV_DispatchThreadID)
{
    const uint2 dstMipSize = max(HZB_GEN_DST_MIP_RESOLUTION, ONEU2);

    if (any(DTid >= dstMipSize)) {
        return;
    }

    const float2 uv = ((float2)DTid + 0.5f) / (float2)dstMipSize;

    const float4 samples = GATHER_RED_TEX(HZB_SRC_MIP, GET_COMMON_SMP(GPU_SamplerID::NEAREST_CLAMP_TO_EDGE), uv);
    const float depth = HZB_GET_FAREST_DEPTH(samples);

    HZB_DST_MIP_UAV[DTid] = depth;
}
