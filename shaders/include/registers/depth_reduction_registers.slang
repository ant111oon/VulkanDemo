#ifndef DEPTH_REDUCTION_REGISTERS_H
#define DEPTH_REDUCTION_REGISTERS_H

#include "common/system/sys_constants.slang"


static const uint DEPTH_REDUCTION_CS_GROUP_SIZE = 256;

struct GPU_DepthReductionPushConst
{
    property bool isTexSrc
    {
        get { return (bool)useTexSrc; }
    }

    uint2 srcTexSize;
    uint  srcBufSize;

    float zNear;
    float zFar;

    private uint useTexSrc : 1;
    private uint padding : 31;
};

[[vk::push_constant]] GPU_DepthReductionPushConst DEPTH_REDUCTION_CONST;

#define DEPTH_REDUCTION_SRC_TEX_SIZE DEPTH_REDUCTION_CONST.srcTexSize
#define DEPTH_REDUCTION_SRC_BUF_SIZE DEPTH_REDUCTION_CONST.srcBufSize

#define DEPTH_REDUCTION_Z_NEAR  DEPTH_REDUCTION_CONST.zNear
#define DEPTH_REDUCTION_Z_FAR   DEPTH_REDUCTION_CONST.zFar

#define DEPTH_REDUCTION_IS_TEX_SRC DEPTH_REDUCTION_CONST.isTexSrc


[vk::binding(0, DESC_SET_PER_DRAW)] Texture2D<float>         DEPTH_REDUCTION_SRC_TEX;
[vk::binding(1, DESC_SET_PER_DRAW)] StructuredBuffer<float2> DEPTH_REDUCTION_SRC_BUF;

[vk::binding(2, DESC_SET_PER_DRAW)] RWStructuredBuffer<float2> DEPTH_REDUCTION_DST;


#endif