#ifndef GBUFFER_VS_IN_H
#define GBUFFER_VS_IN_H

#include "registers/common_registers.slang"
#include "registers/gbuffer_registers.slang"

#include "common/geom_stream_incl.slang"


struct GBUFFER_INPUT_DATA
{
    float3 position;
    float3 lnorm;
    float2 uv;
    float4 tangent;
};


GBUFFER_INPUT_DATA PrepareGBufferVSInputData(uint vertID)
{
    GBUFFER_INPUT_DATA data = (GBUFFER_INPUT_DATA)0;

    data.position = GeomStreamLoadPosition(vertID);
    data.lnorm = GeomStreamLoadNormal(vertID);
    data.uv = GeomStreamLoadUV(vertID);
    data.tangent = GeomStreamLoadTangent(vertID);

    return data;
}

#endif