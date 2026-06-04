#ifndef GBUFFER_REGISTERS_H
#define GBUFFER_REGISTERS_H

#include "common_registers.slang"
#include "common/geom_structs.slang"


struct GBUFFER_PER_DRAW_DATA
{
    uint IS_AKILL_PASS;
};

[[vk::push_constant]] GBUFFER_PER_DRAW_DATA GBUFFER_DYN;

#define GBUFFER_IS_AKILL_PASS (GBUFFER_DYN.IS_AKILL_PASS != 0)


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<uint> GBUFFER_INST_ID_QUEUE;

#endif