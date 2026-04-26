#ifndef HZB_REGISTERS_H
#define HZB_REGISTERS_H

#include "common/system/sys_constants.slang"


static const uint HZB_BUILD_CS_GROUP_SIZE = 8;


struct HZB_GEN_PER_DRAW_DATA
{
    uint2 DST_MIP_RESOLUTION;
    uint DST_MIP_IDX;
};

[[vk::push_constant]] HZB_GEN_PER_DRAW_DATA HZB_GEN_DYN;

#define HZB_GEN_IS_DEPTH_BUFFER_COPY_PASS (HZB_GEN_DYN.DST_MIP_IDX == 0)
#define HZB_GEN_DST_MIP_RESOLUTION         HZB_GEN_DYN.DST_MIP_RESOLUTION
#define HZB_GEN_DST_MIP_IDX                HZB_GEN_DYN.DST_MIP_IDX


[vk::binding(0, DESC_SET_PER_DRAW)] Texture2D HZB_SRC_MIPS[];
[vk::binding(1, DESC_SET_PER_DRAW)] RWTexture2D HZB_DST_MIP_UAVS[];

#endif