#ifndef ZPASS_VS_IN_H
#define ZPASS_VS_IN_H

#include "registers/common_registers.slang"
#include "registers/zpass_registers.slang"

#include "common/geom_stream_incl.slang"


struct GPU_ZPassInput
{
    float3 position;
    float2 uv;
};


GPU_ZPassInput PrepareZPassVSInputData(uint vertID, bool fetchUV = true)
{
    GPU_ZPassInput data = (GPU_ZPassInput)0;

    data.position = GeomStreamLoadPosition(vertID);

    if (fetchUV) {
        data.uv = GeomStreamLoadUV(vertID);
    }

    return data;
}

#endif