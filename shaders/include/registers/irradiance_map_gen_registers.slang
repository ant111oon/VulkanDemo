#ifndef IRRADIANCE_MAP_GEN_H
#define IRRADIANCE_MAP_GEN_H

#include "common/system/sys_constants.slang"


struct IRRADIANCE_MAP_PUSH_CONSTS
{
    uint2 ENV_MAP_FACE_SIZE;
};

[[vk::push_constant]] IRRADIANCE_MAP_PUSH_CONSTS PUSH_CONSTS;


[vk::binding(0, DESC_SET_PER_DRAW)] TextureCube IRRADIANCE_MAP_GEN_ENV_MAP;
[vk::binding(1, DESC_SET_PER_DRAW)] RWTexture2DArray IRRADIANCE_MAP_GEN_OUTPUT_UAV;


#endif