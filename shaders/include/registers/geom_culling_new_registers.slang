#ifndef GEOM_CULLING_NEW_REGISTERS_H
#define GEOM_CULLING_NEW_REGISTERS_H

#include "common/system/sys_constants.slang"
#include "common/common_geom.slang"


struct GPU_GeomCullingNewPerDrawData
{
    uint hzbMipsCount;
};

[[vk::push_constant]] GPU_GeomCullingNewPerDrawData GEOM_CULLING_NEW_DYN;

#define GEOM_CULLING_NEW_HZB_MIPS_COUNT GEOM_CULLING_NEW_DYN.hzbMipsCount


[vk::binding(0, DESC_SET_PER_DRAW)] RWStructuredBuffer<GPU_GeomSortKey> GEOM_CULL_VIS_SORT_KEYS_UAV;
[vk::binding(1, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint>            GEOM_CULL_VIS_GEOM_IDS_UAV;

[vk::binding(2, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint>            GEOM_CULL_VIS_INST_COUNTER_UAV;

// .x - offset, .y - count
[vk::binding(3, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint2>           GEOM_CULL_PER_GEOM_MAT_TYPE_RANGES_UAV;


#endif