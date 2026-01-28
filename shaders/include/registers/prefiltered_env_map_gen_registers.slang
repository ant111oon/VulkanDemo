#ifndef PREFILTERED_ENV_MAP_GEN_H
#define PREFILTERED_ENV_MAP_GEN_H

#include "common_registers.slang"


static const uint2 PREFILTERED_ENV_MAP_OUTPUT_SIZE = uint2(128);
static const float PREFILTERED_ENV_MAP_MIP_ROUGHNESS_DELTA = 1.f / (COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT - 1);


struct PREFILTERED_ENV_MAP_PUSH_CONSTS
{
    uint2 ENV_MAP_FACE_SIZE;
    uint  MIP;
    uint  PADDING;
};

[[vk::push_constant]] PREFILTERED_ENV_MAP_PUSH_CONSTS PUSH_CONSTS;


[vk::binding(0, 1)] TextureCube PREFILTERED_ENV_MAP_GEN_ENV_MAP;
[vk::binding(1, 1)] RWTexture2DArray PREFILTERED_ENV_MAP_GEN_OUTPUT_UAV;


#endif