#ifndef ZPASS_VS_IN_H
#define ZPASS_VS_IN_H

#include "registers/common_registers.slang"
#include "registers/zpass_registers.slang"

#include "common/math/half.slang"


struct ZPASS_INPUT_DATA
{
    float3 position;
    float2 uv;
};


ZPASS_INPUT_DATA PrepareZPassVSInputData(uint vertID)
{
    ZPASS_INPUT_DATA inputData = (ZPASS_INPUT_DATA)0;

    const uint offset = vertID * VERTEX_DATA_SIZE_UI;

    inputData.position.xy = UnpackHalf2x16(COMMON_VERTEX_DATA[offset + 0]);
    inputData.position.z = UnpackHalf2x16(COMMON_VERTEX_DATA[offset + 1]).x;

    // Move this code under define for example COMMON_AKILL or to separate shader
    inputData.uv = UnpackHalf2x16(COMMON_VERTEX_DATA[offset + 3]);

    return inputData;
}

#endif