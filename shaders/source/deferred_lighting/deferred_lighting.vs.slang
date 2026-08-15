#include "pass/deferred_lighting/vsin.slang"
#include "pass/deferred_lighting/vsout.slang"

#include "common/common_funcs.slang"


[shader("vertex")]
GPU_DeferredLightingVsOutput main(uint vertID : SV_VertexID)
{
    GPU_DeferredLightingVsOutput result = (GPU_DeferredLightingVsOutput)0;

    result.hpos = float4(GetScreenQuadVertNDC(vertID), 1.f);

    return result;
}