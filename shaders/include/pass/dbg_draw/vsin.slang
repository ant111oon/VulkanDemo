#ifndef DBG_DRAW_VS_IN_H
#define DBG_DRAW_VS_IN_H

#include "registers/dbg_draw_registers.slang"


struct DBG_DRAW_INPUT_DATA
{
    float3 wPos;
};


DBG_DRAW_INPUT_DATA PrepareDbgDrawVSInputData(uint vertID)
{
    DBG_DRAW_INPUT_DATA inputData = (DBG_DRAW_INPUT_DATA)0;

#if defined(DBG_DRAW_LINES_PASS)
    const uint offset = vertID * DBG_LINE_VERTEX_DATA_SIZE_UI;

    inputData.wPos.xy = unpackHalf2x16ToFloat(DBG_DRAW_LINES_VERTEX_BUFFER[offset + 0]);
    inputData.wPos.z = unpackHalf2x16ToFloat(DBG_DRAW_LINES_VERTEX_BUFFER[offset + 1]).x;
#elif defined(DBG_DRAW_TRIANGLES_PASS)
    const uint offset = vertID * DBG_TRIANGLE_VERTEX_DATA_SIZE_UI;

    inputData.wPos.xy = unpackHalf2x16ToFloat(DBG_DRAW_TRIANGLES_VERTEX_BUFFER[offset + 0]);
    inputData.wPos.z = unpackHalf2x16ToFloat(DBG_DRAW_TRIANGLES_VERTEX_BUFFER[offset + 1]).x;
#else
    #error Unknown Debug Draw Pass
#endif

    return inputData;
}

#endif