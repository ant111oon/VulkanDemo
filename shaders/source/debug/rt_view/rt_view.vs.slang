#include "pass/dbg_rt_view/vsin.slang"
#include "pass/dbg_rt_view/vsout.slang"

#include "common/common_funcs.slang"


[shader("vertex")]
GPU_DbgRTViewVsOutput main(uint vertID : SV_VertexID)
{
    GPU_DbgRTViewVsOutput result = (GPU_DbgRTViewVsOutput)0;

    result.hpos = float4(GetScreenQuadVertNDC(vertID), 1.f);

    return result;
}