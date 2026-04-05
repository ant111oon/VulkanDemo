#define DBG_DRAW_TRIANGLES_PASS

#include "registers/dbg_draw_registers.slang"

#include "pass/dbg_draw/vsin.slang"
#include "pass/dbg_draw/vsout.slang"


[shader("vertex")]
DBG_DRAW_VS_OUTPUT main(uint vertID : SV_VertexID, uint instID : SV_VulkanInstanceID)
{
    DBG_DRAW_VS_OUTPUT result = (DBG_DRAW_VS_OUTPUT)0;

    vertID += instID * 3;

    DBG_DRAW_INPUT_DATA inputData = PrepareDbgDrawVSInputData(vertID);

    result.data.color = unpackUnorm4x8ToFloat(DBG_DRAW_TRIANGLES_DATA[instID].COLOR);

    result.hpos = mul(COMMON_CB.VIEW_PROJ_MATRIX, float4(inputData.wPos, 1.f));

    return result; 
}