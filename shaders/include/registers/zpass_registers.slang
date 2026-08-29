#ifndef ZPASS_REGISTERS_H
#define ZPASS_REGISTERS_H

#include "common_registers.slang"
#include "common/common_geom.slang"


struct GPU_ZPassPushConst
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

[[vk::push_constant]] GPU_ZPassPushConst ZPASS_CONST;

#define ZPASS_IS_AKILL_PASS ZPASS_CONST.isAKillPass
#define ZPASS_IS_CSM_PASS   ZPASS_CONST.isCsmPass
#define ZPASS_CSM_CASCADE   ZPASS_CONST.cascade


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<uint> ZPASS_INST_ID_QUEUE;

#endif