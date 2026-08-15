#ifndef CSM_VS_IN_H
#define CSM_VS_IN_H

#include "registers/common_registers.slang"
#include "registers/csm_registers.slang"

#include "common/geom_stream_incl.slang"


struct GPU_CsmInput
{
    float3 position;
    float2 uv;
};


GPU_CsmInput PrepareCSMPassVSInputData(uint vertID, bool fetchUV = true)
{
    GPU_CsmInput data = (GPU_CsmInput)0;

    data.position = GeomStreamLoadPosition(vertID);

    if (fetchUV) {
        data.uv = GeomStreamLoadUV(vertID);
    }

    return data;
}

#endif