#ifndef GBUFFER_VS_IN_H
#define GBUFFER_VS_IN_H

#include "registers/dbg_draw_registers.slang"


struct DBG_DRAW_LINE_INPUT_DATA
{
    float3 wPos;
};


DBG_DRAW_LINE_INPUT_DATA PrepareDbgDrawLineVSInputData(uint vertID)
{
    DBG_DRAW_LINE_INPUT_DATA inputData = (DBG_DRAW_LINE_INPUT_DATA)0;

    const uint offset = vertID * DBG_LINE_VERTEX_DATA_SIZE_UI;

    inputData.wPos.xy = unpackHalf2x16ToFloat(DBG_DRAW_LINES_VERTEX_BUFFER[offset + 0]);
    inputData.wPos.z = unpackHalf2x16ToFloat(DBG_DRAW_LINES_VERTEX_BUFFER[offset + 1]).x;

    return inputData;
}

#endif