#ifndef GEOM_SORTING_REGISTERS_H
#define GEOM_SORTING_REGISTERS_H

#include "common/system/sys_constants.slang"
#include "common/common_geom.slang"


struct GPU_GeomSortPushConst
{
    uint bitOffset;
    uint groupCount;
};

[[vk::push_constant]] GPU_GeomSortPushConst GEOM_SORT_CONST;

#define GEOM_SORT_BIT_OFFSET        GEOM_SORT_CONST.bitOffset
#define GEOM_SORT_GROUP_COUNT       GEOM_SORT_CONST.groupCount


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<GPU_GeomSortKey>   GEOM_SORT_KEY_BUFFER_SRC;
[vk::binding(1, DESC_SET_PER_DRAW)] RWStructuredBuffer<GPU_GeomSortKey> GEOM_SORT_KEY_BUFFER_DST;

[vk::binding(2, DESC_SET_PER_DRAW)] StructuredBuffer<uint>              GEOM_SORT_INST_ID_BUFFER_SRC;
[vk::binding(3, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint>            GEOM_SORT_INST_ID_BUFFER_DST;

[vk::binding(4, DESC_SET_PER_DRAW)] StructuredBuffer<uint>              GEOM_SORT_VIS_INST_COUNT;

[vk::binding(5, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint>            GEOM_SORT_GROUP_OFFSETS;
[vk::binding(6, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint>            GEOM_SORT_BUCKET_BASES;

#endif