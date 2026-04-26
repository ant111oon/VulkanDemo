#ifndef HZB_REGISTERS_H
#define HZB_REGISTERS_H

#include "common/system/sys_constants.slang"


static const uint HZB_BUILD_CS_GROUP_SIZE = 8;


struct HZB_GEN_PUSH_CONSTS
{
    uint2 DST_MIP_RESOLUTION;
    uint DST_MIP_IDX;
};

[[vk::push_constant]] HZB_GEN_PUSH_CONSTS PUSH_CONSTS;

#define HZB_IS_DEPTH_BUFFER_COPY_PASS (PUSH_CONSTS.DST_MIP_IDX == 0)


[vk::binding(0, DESC_SET_PER_DRAW)] Texture2D HZB_SRC_MIPS[];
[vk::binding(1, DESC_SET_PER_DRAW)] RWTexture2D HZB_DST_MIP_UAVS[];

#endif