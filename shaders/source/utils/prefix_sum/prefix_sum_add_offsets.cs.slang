#include "common/common_prefix_sum.slang"

#include "registers/prefix_sum_registers.slang"


[shader("compute")]
[numthreads(PREFIX_SUM_GROUP_SIZE, 1, 1)]
void main(SV_GROUP_ID(groupID), SV_GROUP_THREAD_FLAT_ID(groupThreadID))
{
    const uint localID = groupThreadID;
    const uint blockID = groupID.x;

    const uint blockOffset = PREFIX_SUM_BLOCK_OFFSETS[blockID];

    const uint blockBase = blockID * PREFIX_SUM_BLOCK_SIZE;

    const uint index0 = blockBase + localID;
    const uint index1 = blockBase + localID + PREFIX_SUM_GROUP_SIZE;

    if (index0 < PREFIX_SUM_MAX_ELEMENT_COUNT) {
        PREFIX_SUM_OUTPUT[index0] += blockOffset;
    }

    if (index1 < PREFIX_SUM_MAX_ELEMENT_COUNT) {
        PREFIX_SUM_OUTPUT[index1] += blockOffset;
    }
}