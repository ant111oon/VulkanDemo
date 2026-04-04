#ifndef GBUFFER_VS_IN_H
#define GBUFFER_VS_IN_H

#include "registers/common_registers.slang"
#include "registers/gbuffer_registers.slang"


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

    const uint offset = vertID * COMMON_VERTEX_DATA_SIZE_UI;

    inputData.position.xy = unpackHalf2x16ToFloat(COMMON_VERTEX_DATA[offset + 0]);
    inputData.position.z = unpackHalf2x16ToFloat(COMMON_VERTEX_DATA[offset + 1]).x;

    inputData.lnorm.x = unpackHalf2x16ToFloat(COMMON_VERTEX_DATA[offset + 1]).y;
    inputData.lnorm.yz = unpackHalf2x16ToFloat(COMMON_VERTEX_DATA[offset + 2]);

    inputData.uv = unpackHalf2x16ToFloat(COMMON_VERTEX_DATA[offset + 3]);

    inputData.tangent.xy = unpackHalf2x16ToFloat(COMMON_VERTEX_DATA[offset + 4]);
    inputData.tangent.zw = unpackHalf2x16ToFloat(COMMON_VERTEX_DATA[offset + 5]);

    return inputData;
}

#endif