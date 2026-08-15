#ifndef BACKBUFFER_VS_OUT_H
#define BACKBUFFER_VS_OUT_H


struct GPU_BackbufferVsOutputInterp
{
};


struct GPU_BackbufferVsOutputNoInterp
{
};


struct GPU_BackbufferVsOutput
{
    GPU_BackbufferVsOutputInterp data;
    nointerpolation GPU_BackbufferVsOutputNoInterp flatData;

    float4 hpos : SV_Position;
};

#endif