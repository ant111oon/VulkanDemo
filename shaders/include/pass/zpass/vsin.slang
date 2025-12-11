#ifndef ZPASS_VS_IN_H
#define ZPASS_VS_IN_H

#include "registers/common_registers.slang"
#include "registers/zpass_registers.slang"

#include "math/half.slang"


struct ZPASS_INPUT_DATA
{
    float3 position;
};


ZPASS_INPUT_DATA PrepareZPassVSInputData(uint vertID)
{
    ZPASS_INPUT_DATA inputData = (ZPASS_INPUT_DATA)0;

    static const uint VERTEX_DATA_SIZE_UI4 = 1;
    const uint offset = vertID * VERTEX_DATA_SIZE_UI4;

    inputData.position.xy = UnpackHalf2x16(COMMON_VERTEX_DATA[offset][0]);
    inputData.position.z = UnpackHalf2x16(COMMON_VERTEX_DATA[offset][1]).x;

    return inputData;
}

#endif