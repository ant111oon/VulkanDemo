#include "registers/geom_sort_registers.slang"
#include "registers/common_registers.slang"


#define GEOM_SORT_BUCKET_SIZES GEOM_SORT_BUCKET_OFFSETS

groupshared uint GROUP_BUCKET_TOTALS[GEOM_SORT_RADIX_BUCKET_COUNT];


[numthreads(GEOM_SORT_RADIX_BUCKET_COUNT, 1, 1)]
[shader("compute")]
void main(SV_GROUP_THREAD_FLAT_ID(groupThreadID))
{
    const uint bucket = groupThreadID;

    uint runningOffset = 0;

    for (uint group = 0; group < GEOM_SORT_GROUP_COUNT; ++group) {
        const uint index = group * GEOM_SORT_RADIX_BUCKET_COUNT + bucket;

        const uint count = GEOM_SORT_BUCKET_SIZES[index];

        GEOM_SORT_BUCKET_OFFSETS[index] = runningOffset;

        runningOffset += count;
    }

    GROUP_BUCKET_TOTALS[bucket] = runningOffset;

    GroupMemoryBarrierWithGroupSync();

    if (groupThreadID == 0) {
        uint bucketBase = 0;

        for (uint i = 0; i < GEOM_SORT_RADIX_BUCKET_COUNT; ++i) {
            GEOM_SORT_BUCKET_BASES[i] = bucketBase;

            bucketBase += GROUP_BUCKET_TOTALS[i];
        }
    }
}