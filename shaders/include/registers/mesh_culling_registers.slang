#ifndef MESH_CULLING_REGISTERS_H
#define MESH_CULLING_REGISTERS_H

#include "common_registers.slang"
#include "common/common_structs.slang"


struct MESH_CULLING_BINDLESS_REGISTRY
{
    float3 PAD0;
    uint INST_COUNT;
};

[[vk::push_constant]] MESH_CULLING_BINDLESS_REGISTRY MESH_CULLING_REGISTRY;

[vk::binding(0, 1)] RWStructuredBuffer<COMMON_INDIRECT_DRAW_CMD> MESH_CULL_INDIRECT_DRAW_CMDS_UAV;
[vk::binding(1, 1)] RWStructuredBuffer<uint> MESH_CULL_INDIRECT_DRAW_CMDS_COUNT_UAV;

#endif