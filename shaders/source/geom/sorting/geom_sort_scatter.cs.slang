#include "registers/geom_sort_registers.slang"
#include "registers/common_registers.slang"


groupshared uint GROUP_BUCKET_MASKS[GEOM_SORT_RADIX_MASK_COUNT];


[numthreads(GEOM_SORT_CS_GROUP_SIZE, 1, 1)]
[shader("compute")]
void main(SV_GROUP_ID(groupID), SV_GROUP_THREAD_FLAT_ID(groupThreadID), SV_DISPATCH_THREAD_ID(dispatchThreadID))
{
    for (uint i = groupThreadID; i < GEOM_SORT_RADIX_MASK_COUNT; i += GEOM_SORT_CS_GROUP_SIZE) {
        GROUP_BUCKET_MASKS[i] = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    const uint group = groupID.x;

    const uint wordIndex = groupThreadID / GEOM_SORT_RADIX_MASK_WORD_BITS;
    const uint bitIndex = groupThreadID % GEOM_SORT_RADIX_MASK_WORD_BITS;

    const uint instCount = GEOM_SORT_VIS_INST_COUNT[0];
    const uint instIndex = dispatchThreadID.x;
    const bool isValid = instIndex < instCount;

    GPU_GeomSortKey sortKey;

    uint bucket = 0;
    uint instMaskIndex = 0;

    if (isValid) {
        sortKey = GEOM_SORT_KEY_BUFFER_SRC[instIndex];

        bucket = sortKey.GetDigits(GEOM_SORT_BIT_OFFSET, GEOM_SORT_RADIX_BITS);

        instMaskIndex = bucket * GEOM_SORT_RADIX_MASK_WORD_COUNT + wordIndex;

        InterlockedOr(GROUP_BUCKET_MASKS[instMaskIndex], 1u << bitIndex);
    }

    GroupMemoryBarrierWithGroupSync();

    if (!isValid) {
        return;
    }

    uint localRank = 0;

    for (uint word = 0; word < wordIndex; ++word) {
        const uint maskIndex = bucket * GEOM_SORT_RADIX_MASK_WORD_COUNT + word;

        localRank += countbits(GROUP_BUCKET_MASKS[maskIndex]);
    }

    const uint word = GROUP_BUCKET_MASKS[instMaskIndex];
    const uint bitsBeforeCurrentBit = (1u << bitIndex) - 1u;

    localRank += countbits(word & bitsBeforeCurrentBit);

    const uint groupBucketIndex = group * GEOM_SORT_RADIX_BUCKET_COUNT + bucket;
    const uint previousGroupsCount = GEOM_SORT_BUCKET_OFFSETS[groupBucketIndex];
    const uint bucketBase = GEOM_SORT_BUCKET_BASES[bucket];
    
    const uint dstIndex = bucketBase + previousGroupsCount + localRank;

    const uint instID = GEOM_SORT_INST_ID_BUFFER_SRC[instIndex];

    GEOM_SORT_KEY_BUFFER_DST[dstIndex] = sortKey;
    GEOM_SORT_INST_ID_BUFFER_DST[dstIndex] = instID;
}