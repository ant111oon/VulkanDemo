#include "registers/geom_sort_registers.slang"
#include "registers/common_registers.slang"


#define GEOM_SORT_BUCKET_SIZES GEOM_SORT_BUCKET_OFFSETS

groupshared uint GROUP_HISTOGRAM[GEOM_SORT_RADIX_BUCKET_COUNT];


[numthreads(GEOM_SORT_CS_GROUP_SIZE, 1, 1)]
[shader("compute")]
void main(SV_GROUP_ID(groupID), SV_GROUP_THREAD_FLAT_ID(groupThreadID), SV_DISPATCH_THREAD_ID(dispatchThreadID))
{
    const uint threadID = groupThreadID;

    if (threadID < GEOM_SORT_RADIX_BUCKET_COUNT) {
        GROUP_HISTOGRAM[threadID] = 0;
    }

    GroupMemoryBarrierWithGroupSync();
    
    const uint instCount = GEOM_SORT_VIS_INST_COUNT[0];
    const uint instID = dispatchThreadID.x;

    if (instID < instCount) {
        const GPU_GeomSortKey sortKey = GEOM_SORT_KEY_BUFFER_SRC[instID];
        const uint digit = sortKey.GetDigits(GEOM_SORT_BIT_OFFSET, GEOM_SORT_RADIX_BITS);
        
        InterlockedAdd(GROUP_HISTOGRAM[digit], 1);
    }

    GroupMemoryBarrierWithGroupSync();

    if (threadID < GEOM_SORT_RADIX_BUCKET_COUNT) {
        const uint dstIndex = groupID.x * GEOM_SORT_RADIX_BUCKET_COUNT + threadID;

        GEOM_SORT_BUCKET_SIZES[dstIndex] = GROUP_HISTOGRAM[threadID];
    }
}