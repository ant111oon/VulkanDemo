#ifndef FORWARD_REGISTERS_H
#define FORWARD_REGISTERS_H

#include "common_registers.slang"
#include "common/common_structs.slang"


struct FORWARD_BINDLESS_REGISTRY
{
    uint3 PAD0;
    uint INST_INFO_IDX;
};

[[vk::push_constant]] FORWARD_BINDLESS_REGISTRY FORWARD_REGISTRY;


[vk::binding(0, 1)] StructuredBuffer<uint> FORWARD_INST_INFO_IDS;

#endif