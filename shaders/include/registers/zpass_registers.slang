#ifndef ZPASS_REGISTERS_H
#define ZPASS_REGISTERS_H

#include "common_registers.slang"
#include "common/common_structs.slang"


struct ZPASS_PUSH_CONSTS
{
    uint3 PAD0;
    uint INST_INFO_IDX;
};

[[vk::push_constant]] ZPASS_PUSH_CONSTS PUSH_CONSTS;

[vk::binding(0, 1)] StructuredBuffer<uint> ZPASS_INST_INFO_IDS;

#endif