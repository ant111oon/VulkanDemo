#ifndef DBG_DRAW_LINES_REGISTERS_H
#define DBG_DRAW_LINES_REGISTERS_H

#include "common_registers.slang"


struct GPU_DbgLineData
{
    uint color;
};

static const uint DBG_LINE_VERTEX_COUNT = 2;
static const uint DBG_LINE_VERTEX_DATA_SIZE_UI = 2;

[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<uint> DBG_DRAW_LINES_VERTEX_BUFFER;
[vk::binding(1, DESC_SET_PER_DRAW)] StructuredBuffer<GPU_DbgLineData> DBG_DRAW_LINES_DATA;

#endif