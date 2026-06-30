#ifndef CSM_VS_IN_H
#define CSM_VS_IN_H

#include "registers/common_registers.slang"
#include "registers/csm_registers.slang"

#include "common/geom_stream_incl.slang"


struct CSM_INPUT_DATA
{
    float3 position;
    float2 uv;
};


CSM_INPUT_DATA PrepareCSMPassVSInputData(uint vertID, bool fetchUV = true)
{
    CSM_INPUT_DATA data = (CSM_INPUT_DATA)0;

    data.position = GeomStreamLoadPosition(vertID);

    if (fetchUV) {
        data.uv = GeomStreamLoadUV(vertID);
    }

    return data;
}

#endif