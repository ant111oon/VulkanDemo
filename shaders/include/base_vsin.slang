#ifndef TEST_VS_IN_H
#define TEST_VS_IN_H

#include "half_utils.slang"
#include "common_registers.slang"


struct BASE_BINDLESS_REGISTRY
{
    uint3 PAD0;
    uint  INST_INFO_IDX;
};

[[vk::push_constant]] BASE_BINDLESS_REGISTRY BASE_REGISTRY;


struct BASE_INPUT_DATA
{
    float3 position;
    float3 lnorm;
    float2 texCoord;
};


BASE_INPUT_DATA PrepareBaseVSInputData(uint vertID)
{
    BASE_INPUT_DATA inputData = (BASE_INPUT_DATA)0;

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