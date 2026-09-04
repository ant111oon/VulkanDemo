#include "registers/geom_sorting_registers.slang"
#include "registers/common_registers.slang"


groupshared uint GROUP_BUCKET_TOTALS[GEOM_SORT_RADIX_BUCKET_COUNT];


[numthreads(GEOM_SORT_RADIX_BUCKET_COUNT, 1, 1)]
[shader("compute")]
void main(uint Gid : SV_GroupID, uint GI : SV_GroupIndex, uint DTid : SV_DispatchThreadID)
{
    const uint bucket = GI;

    uint runningOffset = 0;

    for (uint group = 0; group < GEOM_SORT_GROUP_COUNT; ++group) {
        const uint index = group * GEOM_SORT_RADIX_BUCKET_COUNT + bucket;

        const uint count = GEOM_SORT_GROUP_OFFSETS[index];

        GEOM_SORT_GROUP_OFFSETS[index] = runningOffset;

        runningOffset += count;
    }

    GROUP_BUCKET_TOTALS[bucket] = runningOffset;

    GroupMemoryBarrierWithGroupSync();

    if (GI == 0) {
        uint bucketBase = 0;

        for (uint i = 0; i < GEOM_SORT_RADIX_BUCKET_COUNT; ++i) {
            GEOM_SORT_BUCKET_BASES[i] = bucketBase;

            bucketBase += GROUP_BUCKET_TOTALS[i];
        }
    }
}