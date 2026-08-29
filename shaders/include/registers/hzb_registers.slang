#ifndef HZB_REGISTERS_H
#define HZB_REGISTERS_H

#include "common/system/sys_constants.slang"


static const uint HZB_BUILD_CS_GROUP_SIZE = 16;


struct GPU_HzbGenPushConst
{
    uint2 srcMipResolution;
    uint2 dstMipResolution;
};

[[vk::push_constant]] GPU_HzbGenPushConst HZB_GEN_CONST;

#define HZB_GEN_SRC_MIP_RESOLUTION HZB_GEN_CONST.srcMipResolution
#define HZB_GEN_DST_MIP_RESOLUTION HZB_GEN_CONST.dstMipResolution


[vk::binding(0, DESC_SET_PER_DRAW)] Texture2D HZB_SRC_MIP;
[vk::binding(1, DESC_SET_PER_DRAW)] RWTexture2D HZB_DST_MIP_UAV;

#endif