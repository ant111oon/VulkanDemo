#ifndef PREFILTERED_ENV_MAP_GEN_H
#define PREFILTERED_ENV_MAP_GEN_H

#include "common_registers.slang"
#include "common/math/constants.slang"


struct PREFILTERED_ENV_MAP_PUSH_CONSTS
{
    uint2 ENV_MAP_FACE_SIZE;
    uint  MIP;
    uint  PADDING;
};

[[vk::push_constant]] PREFILTERED_ENV_MAP_PUSH_CONSTS PUSH_CONSTS;


[vk::binding(0, 1)] TextureCube PREFILTERED_ENV_MAP_GEN_ENV_MAP;
[vk::binding(1, 1)] RWTexture2D PREFILTERED_ENV_MAP_GEN_OUTPUT_UAV[M3D_CUBEMAP_FACE_COUNT * COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT];


#endif