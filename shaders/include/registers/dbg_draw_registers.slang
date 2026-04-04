#ifndef DBG_DRAW_REGISTERS_H
#define DBG_DRAW_REGISTERS_H

#include "common_registers.slang"


struct DBG_LINE_DATA
{
    uint COLOR;
};


enum DBG_DRAW_PASS_ID
{
    LINES,
    COUNT
};

struct DBG_DRAW_PUSH_CONSTS
{
    DBG_DRAW_PASS_ID PASS_ID;
};

[[vk::push_constant]] DBG_DRAW_PUSH_CONSTS PUSH_CONSTS;


static const uint DBG_LINE_VERTEX_DATA_SIZE_UI = 2;
[vk::binding(0, 1)] StructuredBuffer<uint> DBG_DRAW_LINES_VERTEX_BUFFER;

[vk::binding(1, 1)] StructuredBuffer<DBG_LINE_DATA> DBG_DRAW_LINES_DATA;

#endif