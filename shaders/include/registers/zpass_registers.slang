#ifndef ZPASS_REGISTERS_H
#define ZPASS_REGISTERS_H

#include "common_registers.slang"
#include "common/common_geom.slang"


struct GPU_ZPassPerDrawData
{
    uint cascade;
    uint isAKillPass;
    uint isCsmPass;
};

[[vk::push_constant]] GPU_ZPassPerDrawData ZPASS_DYN;

#define ZPASS_IS_AKILL_PASS (ZPASS_DYN.isAKillPass != 0)
#define ZPASS_IS_CSM_PASS   (ZPASS_DYN.isCsmPass != 0)
#define ZPASS_CSM_CASCADE   ZPASS_DYN.cascade


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<uint> ZPASS_INST_ID_QUEUE;

#endif