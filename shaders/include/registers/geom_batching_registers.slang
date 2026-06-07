#ifndef GEOM_BATCHING_REGISTERS_H
#define GEOM_BATCHING_REGISTERS_H

#include "common/system/sys_constants.slang"
#include "common/geom_structs.slang"


struct GEOM_BATCH_PER_DRAW_DATA
{
    uint PADDING;
};

[[vk::push_constant]] GEOM_BATCH_PER_DRAW_DATA GEOM_BATCH_DYN;


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<uint> GEOM_BATCH_VIS_INST_ID_QUEUE;
[vk::binding(1, DESC_SET_PER_DRAW)] StructuredBuffer<uint> GEOM_BATCH_VIS_INST_ID_QUEUE_SIZE;

[vk::binding(2, DESC_SET_PER_DRAW)] RWStructuredBuffer<GEOM_BATCH> GEOM_BATCH_BATCH_QUEUE_UAV;
[vk::binding(3, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_BATCH_BATCH_QUEUE_SIZE_UAV;

[vk::binding(4, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_BATCH_SORTED_VIS_INST_ID_QUEUE_UAV;
[vk::binding(5, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_BATCH_SORTED_VIS_INST_ID_QUEUE_SIZE_UAV;

[vk::binding(6, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_BATCH_DRAW_CMD_GEN_DISPATCH_CMD_UAV;

#endif