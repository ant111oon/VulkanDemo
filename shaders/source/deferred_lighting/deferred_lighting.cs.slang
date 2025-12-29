#include "common/common_defines.slang"

#include "registers/deferred_lighting_registers.slang"
#include "registers/common_registers.slang"

#include "common/brdf.slang"


float4 ComputeLighting(in uint2 DTid)
{
    return LOAD_TEX(DEFERRED_LIGHTING_GBUFFER_0, int3(DTid, 0));
}


[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    if (any(DTid >= COMMON_CB.SCREEN_SIZE)) {
        return;
    }

    DEFERRED_LIGHTING_OUTPUT_UAV[DTid.xy] = ComputeLighting(DTid);
}