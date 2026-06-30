#ifndef DBG_RT_VIEW_REGISTERS_H
#define DBG_RT_VIEW_REGISTERS_H

#include "common/system/sys_constants.slang"
#include "common/common_structs.slang"


struct DBG_RT_VIEW_PER_DRAW_DATA
{
    DBG_RT_VIEW_TYPE TYPE;
    uint MIP;
    uint FACE;
    uint CSM_CASCADE_IDX;
    float DEPTH_Z_NEAR;
    float DEPTH_Z_FAR;
};

[[vk::push_constant]] DBG_RT_VIEW_PER_DRAW_DATA DBG_RT_VIEW_DYN;

#define DBG_RT_VIEW_MIP         DBG_RT_VIEW_DYN.MIP
#define DBG_RT_VIEW_FACE        DBG_RT_VIEW_DYN.FACE
#define DBG_RT_DEPTH_Z_NEAR     DBG_RT_VIEW_DYN.DEPTH_Z_NEAR
#define DBG_RT_DEPTH_Z_FAR      DBG_RT_VIEW_DYN.DEPTH_Z_FAR
#define DBG_RT_CSM_CASCADE_IDX  DBG_RT_VIEW_DYN.CSM_CASCADE_IDX


[vk::binding(0, DESC_SET_PER_DRAW)] Texture2D DBG_RT_VIEW_COMMON_DEPTH;
[vk::binding(1, DESC_SET_PER_DRAW)] Texture2D DBG_RT_VIEW_COMMON_HZB;

[vk::binding(2, DESC_SET_PER_DRAW)] Texture2D DBG_RT_VIEW_GBUFFER_0;
[vk::binding(3, DESC_SET_PER_DRAW)] Texture2D DBG_RT_VIEW_GBUFFER_1;
[vk::binding(4, DESC_SET_PER_DRAW)] Texture2D DBG_RT_VIEW_GBUFFER_2;
[vk::binding(5, DESC_SET_PER_DRAW)] Texture2D DBG_RT_VIEW_GBUFFER_3;

[vk::binding(6, DESC_SET_PER_DRAW)] TextureCube DBG_RT_VIEW_IRRADIANCE_MAP;
[vk::binding(7, DESC_SET_PER_DRAW)] TextureCube DBG_RT_VIEW_PREFILTERED_ENV_MAP;
[vk::binding(8, DESC_SET_PER_DRAW)] Texture2D   DBG_RT_VIEW_BRDF_LUT;

[vk::binding(9, DESC_SET_PER_DRAW)] TextureCube DBG_RT_VIEW_SKYBOX;

[vk::binding(10, DESC_SET_PER_DRAW)] Texture2DArray DBG_RT_VIEW_CSM;

#endif