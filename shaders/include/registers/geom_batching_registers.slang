#ifndef GEOM_BATCHING_REGISTERS_H
#define GEOM_BATCHING_REGISTERS_H

#include "common/system/sys_constants.slang"
#include "common/common_geom.slang"

#include "common/common_indirect_cmd.slang"


struct GPU_GeomBatchPushConst
{
    uint cmdWriteOffsets[GPU_GeomMatType::COUNT];
};

[[vk::push_constant]] GPU_GeomBatchPushConst GEOM_BATCH_CONST;


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<GPU_GeomSortKey> GEOM_BATCH_SORTED_KEYS;
[vk::binding(1, DESC_SET_PER_DRAW)] StructuredBuffer<uint>            GEOM_BATCH_SORTED_VIS_INST_COUNT;

[vk::binding(2, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_BATCH_START_FLAGS_UAV;

[vk::binding(3, DESC_SET_PER_DRAW)] StructuredBuffer<uint>   GEOM_BATCH_PREFIX;

[vk::binding(4, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_BATCH_FIRST_INST;
[vk::binding(5, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_MAT_TYPE_FIRST_BATCH;
[vk::binding(6, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_BATCH_COUNT;

[vk::binding(7, DESC_SET_PER_DRAW)] RWStructuredBuffer<GPU_CmdDrawIndexedIndirect> GEOM_BATCH_DRAW_CMDS_UAV;
[vk::binding(8, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint>                       GEOM_BATCH_DRAW_CMD_COUNTS_UAV;

#endif