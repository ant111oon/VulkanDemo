#ifndef GBUFFER_REGISTERS_H
#define GBUFFER_REGISTERS_H

#include "common_registers.slang"
#include "common/common_structs.slang"


struct GBUFFER_PUSH_CONSTS
{
    uint3 PAD0;
    uint INST_INFO_IDX;
};

[[vk::push_constant]] GBUFFER_PUSH_CONSTS PUSH_CONSTS;


[vk::binding(0, 1)] StructuredBuffer<uint> GBUFFER_INST_INFO_IDS;

#endif