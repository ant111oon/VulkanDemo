#ifndef MESH_CULLING_REGISTERS_H
#define MESH_CULLING_REGISTERS_H

#include "common_registers.slang"
#include "common/common_structs.slang"


struct MESH_CULLING_PUSH_CONSTS
{
    float3 PAD0;
    uint INST_COUNT;
};

[[vk::push_constant]] MESH_CULLING_PUSH_CONSTS PUSH_CONSTS;


[vk::binding(0, 1)] RWStructuredBuffer<COMMON_INDIRECT_DRAW_CMD> MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_UAV;
[vk::binding(1, 1)] RWStructuredBuffer<uint>                     MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_COUNTER_UAV;

[vk::binding(2, 1)] RWStructuredBuffer<COMMON_INDIRECT_DRAW_CMD> MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_UAV;
[vk::binding(3, 1)] RWStructuredBuffer<uint>                     MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_COUNTER_UAV;

[vk::binding(4, 1)] RWStructuredBuffer<COMMON_INDIRECT_DRAW_CMD> MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_UAV;
[vk::binding(5, 1)] RWStructuredBuffer<uint>                     MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_COUNTER_UAV;

[vk::binding(6, 1)] RWStructuredBuffer<uint> MESH_CULL_OPAQUE_INST_INFO_IDS_UAV;
[vk::binding(7, 1)] RWStructuredBuffer<uint> MESH_CULL_AKILL_INST_INFO_IDS_UAV;
[vk::binding(8, 1)] RWStructuredBuffer<uint> MESH_CULL_TRANSP_INST_INFO_IDS_UAV;

#endif