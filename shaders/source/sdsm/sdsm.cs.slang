#include "registers/sdsm_registers.slang"

#include "common/common_lighting_structs.slang"


float2 GetCascadeZRange(in uint cascade, in float minZ, in float maxZ)
{
    let GetBound = (uint cascade) => {
        return minZ * pow(maxZ / minZ, (float)cascade / CSM_CASCADE_COUNT);
    };

    return float2(GetBound(cascade), GetBound(cascade + 1));
}


[shader("compute")]
[numthreads(CSM_CASCADE_COUNT, 1, 1)]
void main()
{
}