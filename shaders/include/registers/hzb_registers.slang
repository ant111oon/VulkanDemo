#ifndef HZB_REGISTERS_H
#define HZB_REGISTERS_H

#include "common/system/sys_constants.slang"


static const uint HZB_BUILD_CS_GROUP_SIZE = 16;


struct GPU_HzbGenPerDrawData
{
    uint2 srcMipResolution;
    uint2 dstMipResolution;
    uint  dstMipIdx;
};

[[vk::push_constant]] GPU_HzbGenPerDrawData HZB_GEN_DYN;

#define HZB_GEN_IS_DEPTH_BUFFER_COPY_PASS (HZB_GEN_DYN.dstMipIdx == 0)
#define HZB_GEN_SRC_MIP_RESOLUTION         HZB_GEN_DYN.srcMipResolution
#define HZB_GEN_DST_MIP_RESOLUTION         HZB_GEN_DYN.dstMipResolution
#define HZB_GEN_DST_MIP_IDX                HZB_GEN_DYN.dstMipIdx


[vk::binding(0, DESC_SET_PER_DRAW)] Texture2D HZB_SRC_MIPS[];
[vk::binding(1, DESC_SET_PER_DRAW)] RWTexture2D HZB_DST_MIP_UAVS[];

#endif