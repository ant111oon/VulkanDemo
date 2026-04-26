#ifndef GBUFFER_REGISTERS_H
#define GBUFFER_REGISTERS_H

#include "common_registers.slang"
#include "common/common_structs.slang"


struct GBUFFER_PER_DRAW_DATA
{
    uint IS_AKILL_PASS;
    uint INST_INFO_IDX;
};

[[vk::push_constant]] GBUFFER_PER_DRAW_DATA GBUFFER_DYN;

#define GBUFFER_IS_AKILL_PASS GBUFFER_DYN.IS_AKILL_PASS
#define GBUFFER_INST_INFO_IDX GBUFFER_DYN.INST_INFO_IDX


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<uint> GBUFFER_OPAQUE_INST_INFO_IDS;
[vk::binding(1, DESC_SET_PER_DRAW)] StructuredBuffer<uint> GBUFFER_AKILL_INST_INFO_IDS;

#endif