#include "registers/depth_reduction_registers.slang"

#include "common/math/common.slang"


bool IsBackgroundDepth(in float depth)
{
#if defined(ENV_REVERSED_Z)
    return depth <= 0.f;
#else
    return depth >= 1.f;
#endif
}


float2 CmpCombineRange(in float2 a, in float2 b)
{
    return float2(min(a.x, b.x), max(a.y, b.y));
}


groupshared float2 GROUP_SHARED[DEPTH_REDUCTION_CS_GROUP_SIZE];


[shader("compute")]
[numthreads(DEPTH_REDUCTION_CS_GROUP_SIZE, 1, 1)]
void main(
    SV_GROUP_ID(groupID),
    SV_GROUP_THREAD_FLAT_ID(groupThreadFlatID),
    SV_DISPATCH_THREAD_ID(dispatchThreadID)
) {
    const uint localID = groupThreadFlatID;
    const uint globalID = dispatchThreadID.x;

    float2 initRange = float2(M3D_FLT_MAX, M3D_FLT_MIN);

    if (DEPTH_REDUCTION_IS_TEX_SRC) {
        const uint2 srcSize = DEPTH_REDUCTION_SRC_TEX_SIZE;
        const uint2 pixCoord = uint2(globalID % srcSize.x, globalID / srcSize.x);

        if (all(pixCoord < srcSize)) {
            float depth = LOAD_TEX(DEPTH_REDUCTION_SRC_TEX, int3(pixCoord, 0));

            if (!IsBackgroundDepth(depth)) {
                depth = LinearizeDepth(depth, DEPTH_REDUCTION_Z_NEAR, DEPTH_REDUCTION_Z_FAR);
                initRange = FLOAT2(depth);
            }
        }
    } else {
        if (globalID < DEPTH_REDUCTION_SRC_BUF_SIZE) {
            initRange = DEPTH_REDUCTION_SRC_BUF[globalID];
        }
    }

    GROUP_SHARED[localID] = initRange;

    GroupMemoryBarrierWithGroupSync();

    for (uint stride = DEPTH_REDUCTION_CS_GROUP_SIZE / 2; stride > 0; stride >>= 1) {
        if (localID < stride) {
            GROUP_SHARED[localID] = CmpCombineRange(GROUP_SHARED[localID], GROUP_SHARED[localID + stride]);
        }
        
        GroupMemoryBarrierWithGroupSync();
    }

    if (localID == 0) {
        DEPTH_REDUCTION_DST[groupID.x] = GROUP_SHARED[0];
    }
}