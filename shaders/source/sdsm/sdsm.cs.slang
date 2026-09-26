#include "registers/sdsm_registers.slang"

#include "common/common_lighting_structs.slang"


float2 GetSplitZRange(in uint split, in float minZ, in float maxZ)
{
    let GetBound = (uint split) => {
        return minZ * pow(maxZ / minZ, (float)split / CSM_SPLIT_COUNT);
    };

    return float2(GetBound(split), GetBound(split + 1));
}


[shader("compute")]
[numthreads(CSM_SPLIT_COUNT, 1, 1)]
void main()
{
}