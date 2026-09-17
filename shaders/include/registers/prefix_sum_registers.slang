#ifndef PREFIX_SUM_REGISTERS_H
#define PREFIX_SUM_REGISTERS_H

#include "common/system/sys_constants.slang"


struct GPU_PrefixSumPushConst
{
    uint maxElemCount;
};

[[vk::push_constant]] GPU_PrefixSumPushConst PREFIX_SUM_CONST;

#define PREFIX_SUM_MAX_ELEMENT_COUNT PREFIX_SUM_CONST.maxElemCount


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<uint>   PREFIX_SUM_INPUT;
[vk::binding(1, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> PREFIX_SUM_OUTPUT;

[vk::binding(2, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> PREFIX_SUM_BLOCK_SUMS;
[vk::binding(3, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> PREFIX_SUM_BLOCK_OFFSETS;

#endif