#ifndef ZPASS_REGISTERS_H
#define ZPASS_REGISTERS_H

#include "common_registers.slang"
#include "common/common_structs.slang"


struct ZPASS_BINDLESS_REGISTRY
{
    uint3 PAD0;
    uint INST_INFO_IDX;
};

[[vk::push_constant]] ZPASS_BINDLESS_REGISTRY ZPASS_REGISTRY;


[vk::binding(0, 1)] StructuredBuffer<COMMON_INDIRECT_DRAW_CMD> ZPASS_INDIRECT_DRAW_CMDS_UAV;
[vk::binding(1, 1)] StructuredBuffer<uint> ZPASS_INDIRECT_DRAW_CMDS_COUNT;

#endif