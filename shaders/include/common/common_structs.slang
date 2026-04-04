#ifndef COMMON_STRUCTS_H
#define COMMON_STRUCTS_H

struct COMMON_CMD_DRAW_INDEXED_INDIRECT
{
    // NOTE: Don't change order of this variables!!!
    uint INDEX_COUNT;
    uint INSTANCE_COUNT;
    uint FIRST_INDEX;
    int VERTEX_OFFSET;
    uint FIRST_INSTANCE;
};

static const uint COMMON_CMD_DRAW_INDEXED_INDIRECT_SIZE_UINT = sizeof(COMMON_CMD_DRAW_INDEXED_INDIRECT) / sizeof(uint);


#endif