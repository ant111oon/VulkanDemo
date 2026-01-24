#ifndef IRRADIANCE_MAP_GEN_H
#define IRRADIANCE_MAP_GEN_H

#include "common_registers.slang"


static const uint2 IRRADIANCE_MAP_GEN_OUTPUT_SIZE = uint2(32);

static const uint IRRADIANCE_MAP_GEN_GROUP_THREAD_X = 32;
static const uint IRRADIANCE_MAP_GEN_GROUP_THREAD_Y = 32;


struct IRRADIANCE_MAP_BINDLESS_REGISTRY
{
    uint2 ENV_MAP_FACE_SIZE;
    uint2 PADDING;
};

[[vk::push_constant]] IRRADIANCE_MAP_BINDLESS_REGISTRY IRRADIANCE_MAP_REGISTRY;


[vk::binding(0, 1)] TextureCube IRRADIANCE_MAP_GEN_ENV_MAP;
[vk::binding(1, 1)] RWTexture2DArray IRRADIANCE_MAP_GEN_OUTPUT_UAV;


#endif