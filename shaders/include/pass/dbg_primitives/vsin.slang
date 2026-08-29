#ifndef DBG_DRAW_VS_IN_H
#define DBG_DRAW_VS_IN_H

#include "registers/dbg_primitives_registers.slang"


struct GPU_DbgPrimInput
{
    float3 wPos;
    float4 color;
};


GPU_DbgPrimInput PrepareDbgDrawVSInputData(uint instID, uint vertID)
{
    GPU_DbgPrimInput inputData = (GPU_DbgPrimInput)0;

    if (DBG_PRIM_CONST.isLinePass) {
        vertID = instID * DBG_LINE_VERTEX_COUNT + vertID;
        const uint offset = vertID * DBG_LINE_VERTEX_DATA_SIZE_UI;

        inputData.wPos.xy = unpackHalf2x16ToFloat(DBG_DRAW_LINES_VERTEX_BUFFER[offset + 0]);
        inputData.wPos.z = unpackHalf2x16ToFloat(DBG_DRAW_LINES_VERTEX_BUFFER[offset + 1]).x;

        inputData.color = unpackUnorm4x8ToFloat(DBG_DRAW_LINES_DATA[instID].color);
    } else {
        vertID = instID * DBG_TRIANGLE_VERTEX_COUNT + vertID;
        const uint offset = vertID * DBG_TRIANGLE_VERTEX_DATA_SIZE_UI;

        inputData.wPos.xy = unpackHalf2x16ToFloat(DBG_DRAW_TRIANGLES_VERTEX_BUFFER[offset + 0]);
        inputData.wPos.z = unpackHalf2x16ToFloat(DBG_DRAW_TRIANGLES_VERTEX_BUFFER[offset + 1]).x;

        inputData.color = unpackUnorm4x8ToFloat(DBG_DRAW_TRIANGLES_DATA[instID].color);
    }

    return inputData;
}

#endif