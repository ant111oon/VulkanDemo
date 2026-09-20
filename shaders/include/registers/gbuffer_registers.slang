#ifndef GBUFFER_REGISTERS_H
#define GBUFFER_REGISTERS_H

#include "common_registers.slang"
#include "common/common_geom.slang"


struct GPU_GBufferPushConst
{
    property bool isAKillPass
    {
        get { return (bool)akillPass; }
    }

    float4x4 viewProjMatr;

    private uint akillPass : 1;
    private uint padding : 31;
};

[[vk::push_constant]] GPU_GBufferPushConst GBUFFER_CONST;

#define GBUFFER_VIEW_PROJ_MATR GBUFFER_CONST.viewProjMatr
#define GBUFFER_IS_AKILL_PASS  GBUFFER_CONST.isAKillPass


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<uint> GBUFFER_INST_ID_QUEUE;

[vk::binding(1, DESC_SET_PER_DRAW)] StructuredBuffer<GPU_DbgCullInstData> GBUFFER_DBG_CULL_INST_DATA;

#endif