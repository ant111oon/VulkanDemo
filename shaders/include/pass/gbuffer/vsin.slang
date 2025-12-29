#ifndef GBUFFER_VS_IN_H
#define GBUFFER_VS_IN_H

#include "registers/common_registers.slang"
#include "registers/gbuffer_registers.slang"

#include "common/math/half.slang"


struct GBUFFER_INPUT_DATA
{
    float3 position;
    float3 lnorm;
    float2 uv;
    float4 tangent;
};


GBUFFER_INPUT_DATA PrepareGBufferVSInputData(uint vertID)
{
    GBUFFER_INPUT_DATA inputData = (GBUFFER_INPUT_DATA)0;

    const uint offset = vertID * VERTEX_DATA_SIZE_UI;

    inputData.position.xy = UnpackHalf2x16(COMMON_VERTEX_DATA[offset + 0]);
    inputData.position.z = UnpackHalf2x16(COMMON_VERTEX_DATA[offset + 1]).x;

    inputData.lnorm.x = UnpackHalf2x16(COMMON_VERTEX_DATA[offset + 1]).y;
    inputData.lnorm.yz = UnpackHalf2x16(COMMON_VERTEX_DATA[offset + 2]);

    inputData.uv = UnpackHalf2x16(COMMON_VERTEX_DATA[offset + 3]);

    inputData.tangent.xy = UnpackHalf2x16(COMMON_VERTEX_DATA[offset + 4]);
    inputData.tangent.zw = UnpackHalf2x16(COMMON_VERTEX_DATA[offset + 5]);

    return inputData;
}

#endif