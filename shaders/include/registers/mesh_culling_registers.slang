#ifndef MESH_CULLING_REGISTERS_H
#define MESH_CULLING_REGISTERS_H

#include "common/system/sys_constants.slang"


struct MESH_CULLING_PUSH_CONSTS
{
    uint INST_COUNT;
};

[[vk::push_constant]] MESH_CULLING_PUSH_CONSTS PUSH_CONSTS;


[vk::binding(0, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_UAV;
[vk::binding(1, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_UAV;
[vk::binding(2, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_UAV;

[vk::binding(3, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> MESH_CULL_OPAQUE_INST_INFO_IDS_UAV;
[vk::binding(4, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> MESH_CULL_AKILL_INST_INFO_IDS_UAV;
[vk::binding(5, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> MESH_CULL_TRANSP_INST_INFO_IDS_UAV;

#endif