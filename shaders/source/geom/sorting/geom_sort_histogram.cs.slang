#include "registers/geom_sorting_registers.slang"
#include "registers/common_registers.slang"


groupshared uint GROUP_HISTOGRAM[GEOM_SORT_RADIX_BUCKET_COUNT];


[numthreads(GEOM_SORT_CS_GROUP_SIZE, 1, 1)]
[shader("compute")]
void main(uint Gid : SV_GroupID, uint GI : SV_GroupIndex, uint DTid : SV_DispatchThreadID)
{
    if (GI < GEOM_SORT_RADIX_BUCKET_COUNT) {
        GROUP_HISTOGRAM[GI] = 0;
    }

    GroupMemoryBarrierWithGroupSync();
    
    const uint instCount = GEOM_SORT_VIS_INST_COUNT[0];

    if (DTid < instCount) {
        const GPU_GeomSortKey sortKey = GEOM_SORT_KEY_BUFFER_SRC[DTid];
        const uint digit = sortKey.GetDigits(GEOM_SORT_BIT_OFFSET, GEOM_SORT_RADIX_BITS);
        
        InterlockedAdd(GROUP_HISTOGRAM[digit], 1);
    }

    GroupMemoryBarrierWithGroupSync();

    if (GI < GEOM_SORT_RADIX_BUCKET_COUNT) {
        const uint dstIndex = Gid * GEOM_SORT_RADIX_BUCKET_COUNT + GI;

        GEOM_SORT_GROUP_OFFSETS[dstIndex] = GROUP_HISTOGRAM[GI];
    }
}