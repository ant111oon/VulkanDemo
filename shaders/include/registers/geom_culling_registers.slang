#ifndef GEOM_CULLING_REGISTERS_H
#define GEOM_CULLING_REGISTERS_H

#include "common/system/sys_constants.slang"
#include "common/geom_structs.slang"


static const uint GEOM_CULLING_CS_GROUP_SIZE = 1024;


struct GEOM_CULLING_PER_DRAW_DATA
{
    uint  HZB_MIPS_COUNT;
};

[[vk::push_constant]] GEOM_CULLING_PER_DRAW_DATA GEOM_CULLING_DYN;

#define GEOM_CULLING_HZB_MIPS_COUNT GEOM_CULLING_DYN.HZB_MIPS_COUNT


[vk::binding(0, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_VIS_INST_ID_QUEUES_UAV[GEOM_QUEUE_COUNT];
[vk::binding(1, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_VIS_INST_ID_QUEUE_SIZES_UAV[GEOM_QUEUE_COUNT];

[vk::binding(2, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_VIS_FLAGS_UAV;

#endif