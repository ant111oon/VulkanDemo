#ifndef GBUFFER_REGISTERS_H
#define GBUFFER_REGISTERS_H

#include "common_registers.slang"
#include "common/common.slang"


struct GBUFFER_BINDLESS_REGISTRY
{
    uint3 PAD0;
    uint INST_INFO_IDX;
};

[[vk::push_constant]] GBUFFER_BINDLESS_REGISTRY GBUFFER_REGISTRY;


[vk::binding(0, 1)] StructuredBuffer<COMMON_INDIRECT_DRAW_CMD> GBUFFER_INDIRECT_DRAW_CMDS_UAV;
[vk::binding(1, 1)] StructuredBuffer<uint> GBUFFER_INDIRECT_DRAW_CMDS_COUNT;

#endif