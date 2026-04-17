#include "registers/common_registers.slang"
#include "registers/hzb_registers.slang"

#include "common/common_cs.slang"
#include "common/math/constants.slang"
#include "common/system/sys_constants.slang"

#define WAVE_VERSION

#ifndef WAVE_VERSION

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 DTid: SV_DispatchThreadID, uint3 GTid: SV_GroupThreadID)
{
    const uint2 dstMipSize = max(PUSH_CONSTS.SRC_MIP_SIZE >> 1, ONEU2);

    if (any(DTid.xy >= dstMipSize)) {
        return;
    }

    const int2 dstPixCoord = DispatchThreadIDToPixCoord(DTid.xy, dstMipSize);
    const int2 srcPixCoord = dstPixCoord * 2;

    const float d0 = LOAD_TEX(HZB_SRC_MIP, int3(srcPixCoord, 0));
    const float d1 = LOAD_TEX(HZB_SRC_MIP, int3(srcPixCoord + uint2(1, 0), 0));
    const float d2 = LOAD_TEX(HZB_SRC_MIP, int3(srcPixCoord + uint2(0, 1), 0));
    const float d3 = LOAD_TEX(HZB_SRC_MIP, int3(srcPixCoord + uint2(1, 1), 0));

#ifdef ENV_REVERSED_Z
    HZB_DST_MIP_UAV[dstPixCoord] = max(max(d0, d1), max(d2, d3));
#else
    HZB_DST_MIP_UAV[dstPixCoord] = min(min(d0, d1), min(d2, d3));
#endif
}

#else

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 DTid: SV_DispatchThreadID, uint3 GTid: SV_GroupThreadID)
{
    uint2 dstSize = max(PUSH_CONSTS.SRC_MIP_SIZE >> 1, ONEU2);

    if (any(DTid.xy >= dstSize)) {
        return;
    }

    uint2 baseSrc = (DTid.xy * 2);
    uint2 laneOffset = uint2(GTid.xy & 1); // local 2x2 pattern

    uint2 srcCoord = baseSrc + laneOffset;

    float depth = LOAD_TEX(HZB_SRC_MIP, int3(srcCoord, 0)).x;

    // figure out which lanes form the quad
    uint laneIndex = WaveGetLaneIndex();

    // mask out lower 2 bits → groups of 4 lanes
    uint quadBase = laneIndex & ~3;

#ifdef ENV_REVERSED_Z
    float reduced = depth;
    reduced = max(reduced, WaveReadLaneAt(depth, quadBase + 1));
    reduced = max(reduced, WaveReadLaneAt(depth, quadBase + 2));
    reduced = max(reduced, WaveReadLaneAt(depth, quadBase + 3));
#else
    float reduced = depth;
    reduced = min(reduced, WaveReadLaneAt(depth, quadBase + 1));
    reduced = min(reduced, WaveReadLaneAt(depth, quadBase + 2));
    reduced = min(reduced, WaveReadLaneAt(depth, quadBase + 3));
#endif

    // only one lane writes (avoid 4x overdraw)
    if (!IS_FLAG_SET(laneIndex, 3)) {
        HZB_DST_MIP_UAV[DTid.xy] = reduced;
    }
}

#endif