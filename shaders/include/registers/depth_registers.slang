#ifndef DEPTH_REGISTERS_H
#define DEPTH_REGISTERS_H

#include "common_registers.slang"


struct GPU_DepthPushConst
{
    property bool isAKillPass
    {
        get { return (bool)akillPass; }
    }

    private uint akillPass : 1;
    private uint padding : 31;
};

[[vk::push_constant]] GPU_DepthPushConst DEPTH_CONST;

#define DEPTH_VIEW_PROJ_MATR DEPTH_CAM_DATA_CB.viewProjMatr
#define DEPTH_IS_AKILL_PASS  DEPTH_CONST.isAKillPass


[vk::binding(0, DESC_SET_PER_DRAW)] ConstantBuffer<GPU_CommonCameraData> DEPTH_CAM_DATA_CB;
[vk::binding(1, DESC_SET_PER_DRAW)] StructuredBuffer<uint> DEPTH_INST_ID_QUEUE;

#endif