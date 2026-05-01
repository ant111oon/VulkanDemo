#ifndef ZPASS_REGISTERS_H
#define ZPASS_REGISTERS_H

#include "common_registers.slang"
#include "common/common_structs.slang"


struct ZPASS_PER_DRAW_DATA
{
    uint IS_AKILL_PASS;
    uint INST_INFO_IDX;
    uint BUFF_IDX;
};

[[vk::push_constant]] ZPASS_PER_DRAW_DATA ZPASS_DYN;

#define ZPASS_IS_AKILL_PASS (bool)ZPASS_DYN.IS_AKILL_PASS
#define ZPASS_INST_INFO_IDX ZPASS_DYN.INST_INFO_IDX
#define ZPASS_BUFF_IDX      ZPASS_DYN.BUFF_IDX


[vk::binding(0, DESC_SET_PER_DRAW)] StructuredBuffer<uint> ZPASS_OPAQUE_INST_INFO_IDS[2];
[vk::binding(1, DESC_SET_PER_DRAW)] StructuredBuffer<uint> ZPASS_AKILL_INST_INFO_IDS[2];

#endif