#ifndef BACKBUFFER_VS_OUT_H
#define BACKBUFFER_VS_OUT_H


struct BACKBUFFER_VS_OUTPUT_INTERP
{
};


struct BACKBUFFER_VS_OUTPUT_NO_INTERP
{
};


struct BACKBUFFER_VS_OUTPUT
{
    BACKBUFFER_VS_OUTPUT_INTERP data;
    nointerpolation BACKBUFFER_VS_OUTPUT_NO_INTERP flatData;

    float4 hpos : SV_Position;
};

#endif