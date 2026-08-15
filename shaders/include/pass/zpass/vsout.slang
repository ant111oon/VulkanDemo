#ifndef ZPASS_VS_OUT_H
#define ZPASS_VS_OUT_H


struct GPU_ZPassVsOutputInterp
{
    [[vk::location(0)]] float2 uv;
};


struct GPU_ZPassVsOutputNoInterp
{
    [[vk::location(1)]] uint instID;
};


struct GPU_ZPassVsOutput
{
    GPU_ZPassVsOutputInterp data;
    nointerpolation GPU_ZPassVsOutputNoInterp flatData;

    float4 hpos : SV_Position; 
};

#endif