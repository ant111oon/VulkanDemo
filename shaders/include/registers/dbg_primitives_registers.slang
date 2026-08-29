#ifndef DBG_DRAW_TRIANGLES_REGISTERS_H
#define DBG_DRAW_TRIANGLES_REGISTERS_H

#include "common_registers.slang"


struct GPU_DbgTriangleData
{
    uint color;
};

struct GPU_DbgLineData
{
    uint color;
};


static const uint DBG_LINE_VERTEX_COUNT = 2;
static const uint DBG_TRIANGLE_VERTEX_COUNT = 3;

static const uint DBG_LINE_VERTEX_DATA_SIZE_UI = 2;
static const uint DBG_TRIANGLE_VERTEX_DATA_SIZE_UI = 2;


struct GPU_DbgPrimPushConst
{
    property bool isLinePass
    {
        get { return (bool)linePass; }
    }

    private uint linePass : 1;
};

[[vk::push_constant]] GPU_DbgPrimPushConst DBG_PRIM_CONST;


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<uint> DBG_DRAW_TRIANGLES_VERTEX_BUFFER;
[vk::binding(1, DESC_SET_PER_DRAW)] StructuredBuffer<GPU_DbgTriangleData> DBG_DRAW_TRIANGLES_DATA;

[vk::binding(2, DESC_SET_PER_DRAW)] StructuredBuffer<uint> DBG_DRAW_LINES_VERTEX_BUFFER;
[vk::binding(3, DESC_SET_PER_DRAW)] StructuredBuffer<GPU_DbgLineData> DBG_DRAW_LINES_DATA;

#endif