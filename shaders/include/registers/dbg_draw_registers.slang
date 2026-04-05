#ifndef DBG_DRAW_REGISTERS_H
#define DBG_DRAW_REGISTERS_H

#include "common_registers.slang"


struct DBG_LINE_DATA
{
    uint COLOR;
};


struct DBG_TRIANGLE_DATA
{
    uint COLOR;
};


static const uint DBG_LINE_VERTEX_DATA_SIZE_UI = 2;
[vk::binding(0, 1)] StructuredBuffer<uint> DBG_DRAW_LINES_VERTEX_BUFFER;
[vk::binding(1, 1)] StructuredBuffer<DBG_LINE_DATA> DBG_DRAW_LINES_DATA;

static const uint DBG_TRIANGLE_VERTEX_DATA_SIZE_UI = 2;
[vk::binding(2, 1)] StructuredBuffer<uint> DBG_DRAW_TRIANGLES_VERTEX_BUFFER;
[vk::binding(3, 1)] StructuredBuffer<DBG_TRIANGLE_DATA> DBG_DRAW_TRIANGLES_DATA;

#endif