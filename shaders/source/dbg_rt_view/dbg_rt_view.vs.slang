#include "pass/dbg_rt_view/vsin.slang"
#include "pass/dbg_rt_view/vsout.slang"

#include "common/common_funcs.slang"


[shader("vertex")]
DBG_RT_VIEW_VS_OUTPUT main(uint vertID : SV_VertexID)
{
    DBG_RT_VIEW_VS_OUTPUT result = (DBG_RT_VIEW_VS_OUTPUT)0;

    result.hpos = float4(GetScreenQuadVertNDC(vertID), 1.f);

    return result;
}