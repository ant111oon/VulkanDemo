#include "registers/geom_sort_registers.slang"
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
        const GPU_GeomSortKey sortKey = GEOM_SORT_SRC_KEY_BUFFER[DTid];
        const uint digit = sortKey.GetDigit(GEOM_SORT_BIT_OFFSET, GEOM_SORT_RADIX_BITS);
        
        InterlockedAdd(GROUP_HISTOGRAM[digit], 1);
    }

    GroupMemoryBarrierWithGroupSync();

    if (GI < GEOM_SORT_RADIX_BUCKET_COUNT) {
        const uint dstIndex = Gid * GEOM_SORT_RADIX_BUCKET_COUNT + GI;

        GEOM_SORT_GROUP_OFFSETS_UAV[dstIndex] = GROUP_HISTOGRAM[GI];
    }
}