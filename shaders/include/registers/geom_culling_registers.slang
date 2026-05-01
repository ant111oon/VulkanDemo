#ifndef GEOM_CULLING_REGISTERS_H
#define GEOM_CULLING_REGISTERS_H

#include "common/system/sys_constants.slang"


static const uint GEOM_CULLING_CS_GPOUP_SIZE = 64;


struct GEOM_CULLING_PER_DRAW_DATA
{
    uint  INST_COUNT;
    uint  HZB_MIPS_COUNT;
    float VIS_CONTRIBUTION_FALLOFF;
};

[[vk::push_constant]] GEOM_CULLING_PER_DRAW_DATA GEOM_CULLING_DYN;

#define GEOM_CULLING_INST_COUNT               GEOM_CULLING_DYN.INST_COUNT
#define GEOM_CULLING_HZB_MIPS_COUNT           GEOM_CULLING_DYN.HZB_MIPS_COUNT
#define GEOM_CULLING_VIS_CONTRIBUTION_FALLOFF GEOM_CULLING_DYN.VIS_CONTRIBUTION_FALLOFF


[vk::binding(0, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_OPAQUE_DRAW_CMDS_UAV;
[vk::binding(1, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_AKILL_DRAW_CMDS_UAV;
[vk::binding(2, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_TRANSP_DRAW_CMDS_UAV;

[vk::binding(3, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_OPAQUE_INST_INFO_IDS_UAV;
[vk::binding(4, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_AKILL_INST_INFO_IDS_UAV;
[vk::binding(5, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_TRANSP_INST_INFO_IDS_UAV;

[vk::binding(6, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_VIS_FLAGS_UAV;


// NOTE: A little bit complicated logic but that's how it is:
//       GLOBAL and NOT GLOBAL occluders (pass number 1) buffers are the same memory so for that pass we write only in NOT GLOBAL buffers
//       For second pass we write in GLOBAL and NOT GLOBAL buffers to use global data after depth pass in GBUFFER pass
[vk::binding(7, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_GLOBAL_OPAQUE_DRAW_CMDS_UAV;
[vk::binding(8, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_GLOBAL_AKILL_DRAW_CMDS_UAV;
[vk::binding(9, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_GLOBAL_TRANSP_DRAW_CMDS_UAV;

[vk::binding(10, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_GLOBAL_OPAQUE_INST_INFO_IDS_UAV;
[vk::binding(11, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_GLOBAL_AKILL_INST_INFO_IDS_UAV;
[vk::binding(12, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> GEOM_CULL_GLOBAL_TRANSP_INST_INFO_IDS_UAV;

#endif