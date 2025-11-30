#ifndef BASE_CULLING_REGISTERS_H
#define BASE_CULLING_REGISTERS_H

#include "common_registers.slang"
#include "base/common.slang"


struct BASE_CULLING_BINDLESS_REGISTRY
{
    float3 PAD0;
    uint INST_COUNT;
};

[[vk::push_constant]] BASE_CULLING_BINDLESS_REGISTRY CULLING_REGISTRY;


[vk::binding(0, 1)] RWStructuredBuffer<BASE_INDIRECT_DRAW_CMD> BASE_CULL_INDIRECT_DRAW_CMDS_UAV;
[vk::binding(1, 1)] RWStructuredBuffer<uint> BASE_CULL_INDIRECT_DRAW_CMDS_COUNT;

#endif