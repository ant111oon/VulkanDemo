#ifndef GEOM_CULLING_REGISTERS_H
#define GEOM_CULLING_REGISTERS_H

#include "common/system/sys_constants.slang"
#include "common/common_geom.slang"


struct GPU_GeomCullPushConst
{
    property bool isCsmPass
    {
        get { return (bool)csmPass; }
    }

    uint hzbMipsCount;
    uint cascade;

    private uint csmPass : 1;
    private uint padding : 31;
};

[[vk::push_constant]] GPU_GeomCullPushConst GEOM_CULLING_CONST;

#define GEOM_CULLING_HZB_MIPS_COUNT GEOM_CULLING_CONST.hzbMipsCount
#define GEOM_CULLING_IS_CSM_PASS    GEOM_CULLING_CONST.isCsmPass
#define GEOM_CULLING_CSM_CASCADE    GEOM_CULLING_CONST.cascade


[vk::binding(0, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_VIS_INST_ID_QUEUES_UAV[GEOM_QUEUE_COUNT];
[vk::binding(1, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_VIS_INST_ID_QUEUE_SIZES_UAV[GEOM_QUEUE_COUNT];

#endif