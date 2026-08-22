#include "registers/geom_sort_registers.slang"
#include "registers/common_registers.slang"


static const uint MASK_COUNT = GEOM_SORT_RADIX_BUCKET_COUNT * GEOM_SORT_RADIX_MASK_WORD_COUNT;

groupshared uint GROUP_BUCKET_MASKS[MASK_COUNT];


[numthreads(GEOM_SORT_CS_GROUP_SIZE, 1, 1)]
[shader("compute")]
void main(uint Gid : SV_GroupID, uint GI : SV_GroupIndex, uint DTid : SV_DispatchThreadID)
{
    for (uint i = GI; i < MASK_COUNT; i += GEOM_SORT_CS_GROUP_SIZE) {
        GROUP_BUCKET_MASKS[i] = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    const uint instCount = GEOM_SORT_VIS_INST_COUNT[0];
    const bool isValid = DTid < instCount;

    GPU_GeomSortKey sortKey;
    uint instID;

    uint digit = 0;

    if (isValid) {
        sortKey = GEOM_SORT_SRC_KEY_BUFFER[DTid];
        instID = GEOM_SORT_SRC_INST_ID_BUFFER[DTid];

        digit = sortKey.GetDigit(GEOM_SORT_BIT_OFFSET, GEOM_SORT_RADIX_BITS);
        
        const uint wordIndex = GI / GEOM_SORT_RADIX_MASK_WORD_BITS;
        const uint bitIndex = GI % GEOM_SORT_RADIX_MASK_WORD_BITS;

        const uint maskIndex = digit * GEOM_SORT_RADIX_MASK_WORD_COUNT + wordIndex;

        InterlockedOr(GROUP_BUCKET_MASKS[maskIndex], 1u << bitIndex);
    }

    GroupMemoryBarrierWithGroupSync();

    if (!isValid) {
        return;
    }

    const uint myWord = GI / GEOM_SORT_RADIX_MASK_WORD_BITS;
    const uint myBit = GI % GEOM_SORT_RADIX_MASK_WORD_BITS;

    uint localRank = 0;

    for (uint word = 0; word < myWord; ++word) {
        const uint maskIndex = digit * GEOM_SORT_RADIX_MASK_WORD_COUNT + word;

        localRank += countbits(GROUP_BUCKET_MASKS[maskIndex]);
    }

    const uint myMaskIndex = digit * GEOM_SORT_RADIX_MASK_WORD_COUNT + myWord;
    const uint currentWord = GROUP_BUCKET_MASKS[myMaskIndex];

    const uint bitsBeforeMe = (1u << myBit) - 1u;

    localRank += countbits(currentWord & bitsBeforeMe);

    const uint groupBucketIndex = Gid * GEOM_SORT_RADIX_BUCKET_COUNT + digit;
    const uint bucketBase = GEOM_SORT_BUCKET_BASES_UAV[digit];

    const uint previousGroupsCount = GEOM_SORT_GROUP_OFFSETS_UAV[groupBucketIndex];

    const uint dstIndex = bucketBase + previousGroupsCount + localRank;

    GEOM_SORT_DST_KEY_BUFFER[dstIndex] = sortKey;
    GEOM_SORT_DST_INST_ID_BUFFER[dstIndex] = instID;
}