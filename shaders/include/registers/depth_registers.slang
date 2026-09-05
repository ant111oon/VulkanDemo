#ifndef DEPTH_REGISTERS_H
#define DEPTH_REGISTERS_H

#include "common_registers.slang"
#include "common/common_geom.slang"


struct GPU_DepthPushConst
{
    uint cascade;

    property bool isCsmPass
    {
        get { return (bool)csmPass; }
    }

    property bool isAKillPass
    {
        get { return (bool)akillPass; }
    }

    private uint csmPass : 1;
    private uint akillPass : 1;
};

[[vk::push_constant]] GPU_DepthPushConst DEPTH_CONST;

#define DEPTH_IS_AKILL_PASS DEPTH_CONST.isAKillPass
#define DEPTH_IS_CSM_PASS   DEPTH_CONST.isCsmPass
#define DEPTH_CSM_CASCADE   DEPTH_CONST.cascade


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<uint> DEPTH_INST_ID_QUEUE;

#endif