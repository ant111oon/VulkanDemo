#include "registers/dbg_draw_registers.slang"

#include "pass/dbg_draw/vsin.slang"
#include "pass/dbg_draw/vsout.slang"


[shader("vertex")]
DBG_DRAW_LINE_VS_OUTPUT main(uint vertID : SV_VertexID, uint instID : SV_VulkanInstanceID)
{
    DBG_DRAW_LINE_VS_OUTPUT result = (DBG_DRAW_LINE_VS_OUTPUT)0;

    vertID += instID * 2;

    DBG_DRAW_LINE_INPUT_DATA inputData = PrepareDbgDrawLineVSInputData(vertID);

    result.data.color = unpackUnorm4x8ToFloat(DBG_DRAW_LINES_DATA[instID].COLOR).rgb;

    result.hpos = mul(COMMON_CB.VIEW_PROJ_MATRIX, float4(inputData.wPos, 1.f));

    return result; 
}