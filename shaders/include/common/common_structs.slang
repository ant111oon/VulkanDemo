#ifndef COMMON_STRUCTS_H
#define COMMON_STRUCTS_H

struct COMMON_INDIRECT_DRAW_CMD
{
    // NOTE: Don't change order of this variables!!!
    uint INDEX_COUNT;
    uint INSTANCE_COUNT;
    uint FIRST_INDEX;
    int VERTEX_OFFSET;
    uint FIRST_INSTANCE;
};


#endif