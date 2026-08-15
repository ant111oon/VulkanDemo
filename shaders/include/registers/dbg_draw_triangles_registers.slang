#ifndef DBG_DRAW_TRIANGLES_REGISTERS_H
#define DBG_DRAW_TRIANGLES_REGISTERS_H

#include "common_registers.slang"


struct GPU_DbgTriangleData
{
    uint color;
};

static const uint DBG_TRIANGLE_VERTEX_COUNT = 3;
static const uint DBG_TRIANGLE_VERTEX_DATA_SIZE_UI = 2;

[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<uint> DBG_DRAW_TRIANGLES_VERTEX_BUFFER;
[vk::binding(1, DESC_SET_PER_DRAW)] StructuredBuffer<GPU_DbgTriangleData> DBG_DRAW_TRIANGLES_DATA;

#endif