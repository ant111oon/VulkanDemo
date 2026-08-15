#include "pass/backbuffer/vsin.slang"
#include "pass/backbuffer/vsout.slang"

#include "common/common_funcs.slang"


[shader("vertex")]
GPU_BackbufferVsOutput main(uint vertID : SV_VertexID)
{
    GPU_BackbufferVsOutput result = (GPU_BackbufferVsOutput)0;

    result.hpos = float4(GetScreenQuadVertNDC(vertID), 1.f);

    return result;
}