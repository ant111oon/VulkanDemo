#ifndef CULLING_REGISTERS_H
#define CULLING_REGISTERS_H

#include "common_registers.slang"
#include "gbuffer/common.slang"


struct GBUFFER_CULLING_BINDLESS_REGISTRY
{
    float3 PAD0;
    uint INST_COUNT;
};

[[vk::push_constant]] GBUFFER_CULLING_BINDLESS_REGISTRY GBUFFER_CULLING_REGISTRY;


[vk::binding(0, 1)] RWStructuredBuffer<GBUFFER_INDIRECT_DRAW_CMD> GBUFFER_CULL_INDIRECT_DRAW_CMDS_UAV;
[vk::binding(1, 1)] RWStructuredBuffer<uint> GBUFFER_CULL_INDIRECT_DRAW_CMDS_COUNT;

#endif