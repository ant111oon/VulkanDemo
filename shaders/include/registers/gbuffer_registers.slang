#ifndef GBUFFER_REGISTERS_H
#define GBUFFER_REGISTERS_H

#include "common_registers.slang"
#include "common/common_geom.slang"


struct GPU_GBufferPushConst
{
    property bool isAKillPass
    {
        get { return (bool)akillPass; }
    }

    private uint akillPass : 1;
};

[[vk::push_constant]] GPU_GBufferPushConst GBUFFER_CONST;

#define GBUFFER_IS_AKILL_PASS GBUFFER_CONST.isAKillPass


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<uint> GBUFFER_INST_ID_QUEUE;

#endif