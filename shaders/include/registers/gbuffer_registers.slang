#ifndef GBUFFER_REGISTERS_H
#define GBUFFER_REGISTERS_H

#include "common_registers.slang"


struct GPU_GBufferPushConst
{
    GPU_GeomMatType pass;
};

[[vk::push_constant]] GPU_GBufferPushConst GBUFFER_CONST;

#define GBUFFER_VIEW_PROJ_MATR  GBUFFER_CAM_DATA_CB.viewProjMatr
#define GBUFFER_IS_DS_GEOM_PASS (GBUFFER_CONST.pass == GPU_GeomMatType::OPAQUE_DS || GBUFFER_CONST.pass == GPU_GeomMatType::AKILL_DS)


[vk::binding(0, DESC_SET_PER_DRAW)] ConstantBuffer<GPU_CommonCameraData> GBUFFER_CAM_DATA_CB;
[vk::binding(1, DESC_SET_PER_DRAW)] StructuredBuffer<uint> GBUFFER_INST_ID_QUEUE;

[vk::binding(2, DESC_SET_PER_DRAW)] StructuredBuffer<GPU_DbgCullInstData> GBUFFER_DBG_CULL_INST_DATA;

#endif