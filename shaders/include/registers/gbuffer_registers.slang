#ifndef GBUFFER_REGISTERS_H
#define GBUFFER_REGISTERS_H

#include "common_registers.slang"
#include "common/common_geom.slang"


struct GPU_GBufferPerDrawData
{
    uint isAKillPass;
};

[[vk::push_constant]] GPU_GBufferPerDrawData GBUFFER_DYN;

#define GBUFFER_IS_AKILL_PASS (GBUFFER_DYN.isAKillPass != 0)


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<uint> GBUFFER_INST_ID_QUEUE;

#endif