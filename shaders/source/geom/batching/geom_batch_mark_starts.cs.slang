#include "registers/geom_batching_registers.slang"

#include "common/common_geom.slang"


bool IsSameBatch(in GPU_GeomSortKey a, in GPU_GeomSortKey b)
{
    return
        a.matType == b.matType &&
        a.meshID  == b.meshID &&
        a.lodID   == b.lodID;
}


[shader("compute")]
[numthreads(GEOM_BATCH_CS_GROUP_SIZE, 1, 1)]
void main(SV_DISPATCH_THREAD_ID(dispatchThreadID))
{
    const uint instID = dispatchThreadID.x;
    const uint instCount = GEOM_BATCH_SORTED_VIS_INST_COUNT[0];

    if (instID >= instCount) {
        return;
    }

    if (instID == 0) {
        GEOM_BATCH_START_FLAGS_UAV[instID] = 1;
        return;
    }

    const GPU_GeomSortKey currKey = GEOM_BATCH_SORTED_KEYS[instID];
    const GPU_GeomSortKey prevKey = GEOM_BATCH_SORTED_KEYS[instID - 1];

    GEOM_BATCH_START_FLAGS_UAV[instID] = IsSameBatch(currKey, prevKey) ? 0 : 1;
}