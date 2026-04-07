#define DBG_DRAW_LINES_PASS

#include "registers/dbg_draw_registers.slang"

#include "pass/dbg_draw/vsin.slang"
#include "pass/dbg_draw/vsout.slang"


[shader("vertex")]
DBG_DRAW_VS_OUTPUT main(uint vertID : SV_VertexID, uint instID : SV_VulkanInstanceID)
{
    DBG_DRAW_INPUT_DATA inputData = PrepareDbgDrawVSInputData(instID * DBG_LINE_VERTEX_COUNT + vertID);

    DBG_DRAW_VS_OUTPUT result = (DBG_DRAW_VS_OUTPUT)0;

    result.data.color = unpackUnorm4x8ToFloat(DBG_DRAW_LINES_DATA[instID].COLOR);
    result.hpos = mul(COMMON_CB.VIEW_PROJ_MATRIX, float4(inputData.wPos, 1.f));

    return result; 
}