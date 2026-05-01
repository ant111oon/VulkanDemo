#include "registers/common_registers.slang"
#include "registers/hzb_registers.slang"

#include "common/common_cs.slang"
#include "common/math/common.slang"


[shader("compute")]
[numthreads(HZB_BUILD_CS_GROUP_SIZE, HZB_BUILD_CS_GROUP_SIZE, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    const uint mip = HZB_GEN_DST_MIP_IDX;
    const uint2 dstMipSize = max(HZB_GEN_DST_MIP_RESOLUTION, ONEU2);

    if (any(DTid >= dstMipSize)) {
        return;
    }

    if (HZB_GEN_IS_DEPTH_BUFFER_COPY_PASS) {
        HZB_DST_MIP_UAVS[mip][DTid] = LOAD_TEX_ARRAY(HZB_SRC_MIPS, mip, int3(DTid, 0));
        return;
    }

    const uint prevMip = mip - 1u;
    const uint2 srcMipSize = dstMipSize << 1u;

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

#ifdef ENV_REVERSED_Z
    const float depth = saturate(Min4(samples));
#else
    const float depth = saturate(Max4(samples));
#endif

    HZB_DST_MIP_UAVS[mip][dstPixCoord] = depth;
}