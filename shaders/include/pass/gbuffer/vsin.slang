#ifndef GBUFFER_VS_IN_H
#define GBUFFER_VS_IN_H

#include "registers/common_registers.slang"
#include "registers/gbuffer_registers.slang"

#include "math/half.slang"


struct GBUFFER_INPUT_DATA
{
    float3 position;
    float3 lnorm;
    float2 texCoord;
};


GBUFFER_INPUT_DATA PrepareGBufferVSInputData(uint vertID)
{
    GBUFFER_INPUT_DATA inputData = (GBUFFER_INPUT_DATA)0;

    static const uint VERTEX_DATA_SIZE_UI4 = 1;
    const uint offset = vertID * VERTEX_DATA_SIZE_UI4;

    inputData.position.xy = UnpackHalf2x16(COMMON_VERTEX_DATA[offset][0]);
    inputData.position.z = UnpackHalf2x16(COMMON_VERTEX_DATA[offset][1]).x;

    inputData.lnorm.x = UnpackHalf2x16(COMMON_VERTEX_DATA[offset][1]).y;
    inputData.lnorm.yz = UnpackHalf2x16(COMMON_VERTEX_DATA[offset][2]);

    inputData.texCoord = UnpackHalf2x16(COMMON_VERTEX_DATA[offset][3]);

    return inputData;
}

#endif