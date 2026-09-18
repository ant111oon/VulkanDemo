#include "registers/geom_batching_registers.slang"
#include "registers/common_registers.slang"

#include "common/common_geom.slang"


[shader("compute")]
[numthreads(GEOM_BATCH_CS_GROUP_SIZE, 1, 1)]
void main(SV_DISPATCH_THREAD_ID(dispatchThreadID))
{
    const uint index = dispatchThreadID.x;
    const uint visibleCount = GEOM_BATCH_SORTED_VIS_INST_COUNT[0];

    if (index == 0 && visibleCount == 0) {
        GEOM_BATCH_COUNT[0] = 0;
        return;
    }

    if (index >= visibleCount) {
        return;
    }

    const bool isBatchStart = GEOM_BATCH_START_FLAGS_UAV[index] != 0;
    const uint prefix = GEOM_BATCH_PREFIX[index];

    if (isBatchStart) {
        GEOM_BATCH_FIRST_INST[prefix] = index;
    }

    // The last active instance can calculate the total batch count.
    if (index == visibleCount - 1) {
        GEOM_BATCH_COUNT[0] = prefix + isBatchStart;
    }
}