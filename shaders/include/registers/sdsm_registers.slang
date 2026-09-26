#ifndef SDSM_REGISTERS_H
#define SDSM_REGISTERS_H

#include "common_registers.slang"

#include "common/system/sys_constants.slang"


struct GPU_SdsmPushConst
{
};

[[vk::push_constant]] GPU_SdsmPushConst SDSM_CONST;


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<float2> SDSM_VIEW_Z_RANGE;

#endif