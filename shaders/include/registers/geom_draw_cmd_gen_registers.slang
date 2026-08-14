#ifndef GEOM_DRAW_CMD_GEN_REGISTERS_H
#define GEOM_DRAW_CMD_GEN_REGISTERS_H

#include "common/system/sys_constants.slang"
#include "common/common_geom.slang"


struct GEOM_DRAW_CMD_GEN_PER_DRAW_DATA
{
    uint padding;
};

[[vk::push_constant]] GEOM_DRAW_CMD_GEN_PER_DRAW_DATA GEOM_DRAW_CMD_GEN_DYN;


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<GPU_GeomBatch> GEOM_DRAW_CMD_GEN_BATCH_QUEUE;
[vk::binding(1, DESC_SET_PER_DRAW)] StructuredBuffer<uint> GEOM_DRAW_CMD_GEN_BATCH_QUEUE_SIZE;

[vk::binding(2, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_DRAW_CMD_GEN_CMD_QUEUE_UAV;

#endif