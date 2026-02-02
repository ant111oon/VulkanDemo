#ifndef IRRADIANCE_MAP_GEN_H
#define IRRADIANCE_MAP_GEN_H

#include "common_registers.slang"


struct IRRADIANCE_MAP_PUSH_CONSTS
{
    uint2 ENV_MAP_FACE_SIZE;
    uint2 PADDING;
};

[[vk::push_constant]] IRRADIANCE_MAP_PUSH_CONSTS PUSH_CONSTS;


[vk::binding(0, 1)] TextureCube IRRADIANCE_MAP_GEN_ENV_MAP;
[vk::binding(1, 1)] RWTexture2DArray IRRADIANCE_MAP_GEN_OUTPUT_UAV;


#endif