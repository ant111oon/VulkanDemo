#ifndef SM_VS_IN_H
#define SM_VS_IN_H

#include "registers/common_registers.slang"
#include "registers/sm_registers.slang"

#include "common/geom_stream_incl.slang"


struct SM_INPUT_DATA
{
    float3 position;
    float2 uv;
};


SM_INPUT_DATA PrepareSMPassVSInputData(uint vertID, bool fetchUV = true)
{
    SM_INPUT_DATA data = (SM_INPUT_DATA)0;

    data.position = GeomStreamLoadPosition(vertID);

    if (fetchUV) {
        data.uv = GeomStreamLoadUV(vertID);
    }

    return data;
}

#endif