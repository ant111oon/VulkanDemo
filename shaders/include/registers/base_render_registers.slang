#ifndef BASE_RENDER_REGISTERS_H
#define BASE_RENDER_REGISTERS_H

#include "common_registers.slang"
#include "base/common.slang"


struct BASE_RENDER_BINDLESS_REGISTRY
{
    uint3 PAD0;
    uint INST_INFO_IDX;
};

[[vk::push_constant]] BASE_RENDER_BINDLESS_REGISTRY BASE_RENDER_REGISTRY;


[vk::binding(0, 1)] StructuredBuffer<BASE_INDIRECT_DRAW_CMD> BASE_RENDER_INDIRECT_DRAW_CMDS_UAV;
[vk::binding(1, 1)] StructuredBuffer<uint> BASE_RENDER_INDIRECT_DRAW_CMDS_COUNT;

#endif