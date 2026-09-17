#include "common/common_prefix_sum.slang"

#include "registers/prefix_sum_registers.slang"


groupshared uint GROUP_SHARED[PREFIX_SUM_BLOCK_SIZE];


[shader("compute")]
[numthreads(PREFIX_SUM_GROUP_SIZE, 1, 1)]
void main(SV_GROUP_ID(groupID), SV_GROUP_THREAD_FLAT_ID(groupThreadID))
{
    const uint localID = groupThreadID;

    const uint blockBase = groupID.x * PREFIX_SUM_BLOCK_SIZE;

    const uint index0 = blockBase + localID;
    const uint index1 = blockBase + localID + PREFIX_SUM_GROUP_SIZE;

    GROUP_SHARED[localID]                         = index0 < PREFIX_SUM_MAX_ELEMENT_COUNT ? PREFIX_SUM_INPUT[index0] : 0;
    GROUP_SHARED[localID + PREFIX_SUM_GROUP_SIZE] = index1 < PREFIX_SUM_MAX_ELEMENT_COUNT ? PREFIX_SUM_INPUT[index1] : 0;

    GroupMemoryBarrierWithGroupSync();

    uint offset = 1;

    for (uint activeThreads = PREFIX_SUM_BLOCK_SIZE >> 1; activeThreads > 0; activeThreads >>= 1) {
        if (localID < activeThreads) {
            const uint left  = offset * (2 * localID + 1) - 1;
            const uint right = offset * (2 * localID + 2) - 1;

            GROUP_SHARED[right] += GROUP_SHARED[left];
        }

        GroupMemoryBarrierWithGroupSync();

        offset <<= 1;
    }

    if (localID == 0) {
        PREFIX_SUM_BLOCK_SUMS[groupID.x] = GROUP_SHARED[PREFIX_SUM_BLOCK_SIZE - 1];

        GROUP_SHARED[PREFIX_SUM_BLOCK_SIZE - 1] = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    for (uint activeThreads = 1; activeThreads < PREFIX_SUM_BLOCK_SIZE; activeThreads <<= 1) {
        offset >>= 1;

        if (localID < activeThreads) {
            const uint left = offset * (2 * localID + 1) - 1;
            const uint right = offset * (2 * localID + 2) - 1;

            const uint leftValue = GROUP_SHARED[left];

            GROUP_SHARED[left] = GROUP_SHARED[right];
            GROUP_SHARED[right] += leftValue;
        }

        GroupMemoryBarrierWithGroupSync();
    }

    if (index0 < PREFIX_SUM_MAX_ELEMENT_COUNT) {
        PREFIX_SUM_OUTPUT[index0] = GROUP_SHARED[localID];
    }

    if (index1 < PREFIX_SUM_MAX_ELEMENT_COUNT) {
        PREFIX_SUM_OUTPUT[index1] = GROUP_SHARED[localID + PREFIX_SUM_GROUP_SIZE];
    }
}