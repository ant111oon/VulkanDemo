#ifndef GBUFFER_REGISTERS_H
#define GBUFFER_REGISTERS_H

#include "common_registers.slang"
#include "common/common_structs.slang"


struct GBUFFER_BINDLESS_REGISTRY
{
    uint3 PAD0;
    uint INST_INFO_IDX;
};

[[vk::push_constant]] GBUFFER_BINDLESS_REGISTRY GBUFFER_REGISTRY;


[vk::binding(0, 1)] StructuredBuffer<uint> GBUFFER_INST_INFO_IDS;

#endif