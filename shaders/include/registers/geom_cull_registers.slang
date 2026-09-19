#ifndef GEOM_CULL_REGISTERS_H
#define GEOM_CULL_REGISTERS_H

#include "common/system/sys_constants.slang"
#include "common/common_geom.slang"


struct GPU_GeomCullPushConst
{
    property bool isFrustumCullingEnabled
    {
        get { return (bool)frustumCullingEnabled; }
    }

    property bool isHZBCullingEnabled
    {
        get { return (bool)hzbCullingEnabled; }
    }

    GPU_Frustum frustum;
    float4x4 viewMatr;
    float4x4 viewProjMatrPrev;

    uint hzbSizeXMip0 : 12;
    uint hzbSizeYMip0 : 12;
    uint hzbMipsCount : 4;
    
    private uint frustumCullingEnabled : 1;
    private uint hzbCullingEnabled : 1;

    private uint padding : 2;
};

[[vk::push_constant]] GPU_GeomCullPushConst GEOM_CULL_CONST;


#define GEOM_CULL_FRUSTUM             GEOM_CULL_CONST.frustum
#define GEOM_CULL_VIEW_MATR           GEOM_CULL_CONST.viewMatr
#define GEOM_CULL_VIEW_PROJ_MATR_PREV GEOM_CULL_CONST.viewProjMatrPrev
#define GEOM_CULL_HZB_MIPS_COUNT      GEOM_CULL_CONST.hzbMipsCount
#define GEOM_CULL_HZB_MIP_0_SIZE      uint2(GEOM_CULL_CONST.hzbSizeXMip0, GEOM_CULL_CONST.hzbSizeYMip0)

#define GEOM_CULL_IS_FRUSTUM_CULLING_ENABLED GEOM_CULL_CONST.isFrustumCullingEnabled
#define GEOM_CULL_IS_HZB_CULLING_ENABLED     GEOM_CULL_CONST.isHZBCullingEnabled


[vk::binding(0, DESC_SET_PER_DRAW)] Texture2D GEOM_CULL_HZB;

[vk::binding(1, DESC_SET_PER_DRAW)] RWStructuredBuffer<GPU_GeomSortKey> GEOM_CULL_VIS_INST_SORT_KEYS;
[vk::binding(2, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint>            GEOM_CULL_VIS_INST_IDS;

[vk::binding(3, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint>            GEOM_CULL_VIS_INST_COUNTER;

#endif