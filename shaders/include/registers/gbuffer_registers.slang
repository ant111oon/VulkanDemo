#ifndef GBUFFER_REGISTERS_H
#define GBUFFER_REGISTERS_H

#include "common_registers.slang"
#include "common/common_structs.slang"


struct GBUFFER_PUSH_CONSTS
{
    uint IS_AKILL_PASS;
    uint INST_INFO_IDX;
};

[[vk::push_constant]] GBUFFER_PUSH_CONSTS PUSH_CONSTS;


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<uint> GBUFFER_OPAQUE_INST_INFO_IDS;
[vk::binding(1, DESC_SET_PER_DRAW)] StructuredBuffer<uint> GBUFFER_AKILL_INST_INFO_IDS;

#endif