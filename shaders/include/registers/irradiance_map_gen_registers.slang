#ifndef IRRADIANCE_MAP_GEN_H
#define IRRADIANCE_MAP_GEN_H

#include "common/system/sys_constants.slang"


struct GPU_IrradianceMapPushConst
{
    uint2 envMapFaceSize;
};

[[vk::push_constant]] GPU_IrradianceMapPushConst IRRADIANCE_MAP_CONST;

#define IRRADIANCE_MAP_ENV_MAP_FACE_SIZE IRRADIANCE_MAP_CONST.envMapFaceSize


[vk::binding(0, DESC_SET_PER_DRAW)] TextureCube IRRADIANCE_MAP_GEN_ENV_MAP;
[vk::binding(1, DESC_SET_PER_DRAW)] RWTexture2DArray IRRADIANCE_MAP_GEN_OUTPUT_UAV;


#endif