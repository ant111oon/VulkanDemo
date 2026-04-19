#include "registers/common_registers.slang"
#include "registers/hzb_registers.slang"

#include "common/common_cs.slang"
#include "common/math/constants.slang"


[shader("compute")]
[numthreads(HZB_BUILD_CS_GROUP_SIZE, HZB_BUILD_CS_GROUP_SIZE, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    const uint mipIdx = PUSH_CONSTS.DST_MIP_IDX;
    const uint2 dstMipSize = max(PUSH_CONSTS.DST_MIP_RESOLUTION, ONEU2);

    if (any(DTid >= dstMipSize)) {
        return;
    }

    if (HZB_IS_DEPTH_BUFFER_COPY_PASS) {
        HZB_DST_MIP_UAVS[mipIdx][DTid] = LOAD_TEX_ARRAY(HZB_SRC_MIPS, mipIdx, int3(DTid, 0));
        return;
    }

    const uint2 srcMipSize = dstMipSize << 1u;

    const int2 dstPixCoord = DispatchThreadIDToPixCoord(DTid, dstMipSize);
    const int2 srcPixCoord = min(dstPixCoord * 2, srcMipSize - 1);

    const float d0 = LOAD_TEX_ARRAY(HZB_SRC_MIPS, mipIdx - 1, int3(srcPixCoord, 0)).r;
    const float d1 = LOAD_TEX_ARRAY(HZB_SRC_MIPS, mipIdx - 1, int3(min(srcPixCoord + int2(1, 0), srcMipSize - 1), 0)).r;
    const float d2 = LOAD_TEX_ARRAY(HZB_SRC_MIPS, mipIdx - 1, int3(min(srcPixCoord + int2(0, 1), srcMipSize - 1), 0)).r;
    const float d3 = LOAD_TEX_ARRAY(HZB_SRC_MIPS, mipIdx - 1, int3(min(srcPixCoord + int2(1, 1), srcMipSize - 1), 0)).r;

#ifdef ENV_REVERSED_Z
    HZB_DST_MIP_UAVS[mipIdx][dstPixCoord] = saturate(max(max(d0, d1), max(d2, d3)));
#else
    HZB_DST_MIP_UAVS[mipIdx][dstPixCoord] = saturate(min(min(d0, d1), min(d2, d3)));
#endif
}