#ifndef GEOM_CULLING_REGISTERS_H
#define GEOM_CULLING_REGISTERS_H

#include "common/system/sys_constants.slang"
#include "common/common_geom.slang"


struct GPU_GeomCullingPerDrawData
{
    uint  hzbMipsCount;
};

[[vk::push_constant]] GPU_GeomCullingPerDrawData GEOM_CULLING_DYN;

#define GEOM_CULLING_HZB_MIPS_COUNT GEOM_CULLING_DYN.hzbMipsCount


[vk::binding(0, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_VIS_INST_ID_QUEUES_UAV[GEOM_QUEUE_COUNT];
[vk::binding(1, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_VIS_INST_ID_QUEUE_SIZES_UAV[GEOM_QUEUE_COUNT];

#endif