#ifndef DEPTH_VS_IN_H
#define DEPTH_VS_IN_H

#include "registers/common_registers.slang"
#include "registers/depth_registers.slang"

#include "common/geom_stream_incl.slang"


struct GPU_DepthInput
{
    float3 position;
    float2 uv;
};


GPU_DepthInput PrepareDepthVSInputData(uint vertID, bool fetchUV = true)
{
    GPU_DepthInput data = (GPU_DepthInput)0;

    data.position = GeomStreamLoadPosition(vertID);

    if (fetchUV) {
        data.uv = GeomStreamLoadUV(vertID);
    }

    return data;
}

#endif