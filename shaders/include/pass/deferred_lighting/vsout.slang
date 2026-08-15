#ifndef DEFERRED_LIGHTING_VS_OUT_H
#define DEFERRED_LIGHTING_VS_OUT_H


struct GPU_DeferredLightingVsOutputInterp
{
};


struct GPU_DeferredLightingVsOutputNoInterp
{
};


struct GPU_DeferredLightingVsOutput
{
    GPU_DeferredLightingVsOutputInterp data;
    nointerpolation GPU_DeferredLightingVsOutputNoInterp flatData;

    float4 hpos : SV_Position;
};

#endif