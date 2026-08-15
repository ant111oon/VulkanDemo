#include "pass/post_processing/vsin.slang"
#include "pass/post_processing/vsout.slang"

#include "common/common_funcs.slang"


[shader("vertex")]
GPU_PostProcVsOutput main(uint vertID : SV_VertexID)
{
    GPU_PostProcVsOutput result = (GPU_PostProcVsOutput)0;

    result.hpos = float4(GetScreenQuadVertNDC(vertID), 1.f);

    return result;
}