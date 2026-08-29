#ifndef PREFILTERED_ENV_MAP_GEN_H
#define PREFILTERED_ENV_MAP_GEN_H

#include "common_registers.slang"
#include "common/math/constants.slang"


struct GPU_PrefilteredEnvMapPushConst
{
    uint2 envMapFaceSize;
    uint  mip;
};

[[vk::push_constant]] GPU_PrefilteredEnvMapPushConst PREFILTERED_ENV_MAP_CONST;

#define PREFILTERED_ENV_MAP_FACE_SIZE PREFILTERED_ENV_MAP_CONST.envMapFaceSize
#define PREFILTERED_ENV_MAP_MIP       PREFILTERED_ENV_MAP_CONST.mip


[vk::binding(0, DESC_SET_PER_DRAW)] TextureCube PREFILTERED_ENV_MAP_GEN_ENV_MAP;
[vk::binding(1, DESC_SET_PER_DRAW)] RWTexture2D PREFILTERED_ENV_MAP_GEN_OUTPUT_UAV[M3D_CUBEMAP_FACE_COUNT];


#endif