#ifndef ZPASS_VS_IN_H
#define ZPASS_VS_IN_H

#include "registers/common_registers.slang"
#include "registers/zpass_registers.slang"

#include "common/geom_stream_incl.slang"


struct ZPASS_INPUT_DATA
{
    float3 position;
    float2 uv;
};


ZPASS_INPUT_DATA PrepareZPassVSInputData(uint vertID)
{
    ZPASS_INPUT_DATA data = (ZPASS_INPUT_DATA)0;

    data.position = GeomStreamLoadPosition(vertID);
    data.uv = GeomStreamLoadUV(vertID);

    return data;
}

#endif