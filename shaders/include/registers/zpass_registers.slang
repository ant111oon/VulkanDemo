#ifndef ZPASS_REGISTERS_H
#define ZPASS_REGISTERS_H

#include "common_registers.slang"
#include "common/geom_structs.slang"


struct ZPASS_PER_DRAW_DATA
{
    uint IS_AKILL_PASS;
};

[[vk::push_constant]] ZPASS_PER_DRAW_DATA ZPASS_DYN;

#define ZPASS_IS_AKILL_PASS (ZPASS_DYN.IS_AKILL_PASS != 0)


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<uint> ZPASS_INST_ID_QUEUE;

#endif