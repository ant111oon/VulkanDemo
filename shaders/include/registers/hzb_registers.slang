#ifndef HZB_REGISTERS_H
#define HZB_REGISTERS_H

#include "common/system/sys_constants.slang"


struct HZB_GEN_PUSH_CONSTS
{
    uint2 SRC_MIP_SIZE;
};

[[vk::push_constant]] HZB_GEN_PUSH_CONSTS PUSH_CONSTS;


[vk::binding(0, DESC_SET_PER_DRAW)] Texture2D HZB_SRC_MIP;
[vk::binding(1, DESC_SET_PER_DRAW)] RWTexture2D HZB_DST_MIP_UAV;

#endif