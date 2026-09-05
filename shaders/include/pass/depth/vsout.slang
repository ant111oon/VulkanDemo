#ifndef DEPTH_VS_OUT_H
#define DEPTH_VS_OUT_H


struct GPU_DepthVsOutputInterp
{
    [[vk::location(0)]] float2 uv;
};


struct GPU_DepthVsOutputNoInterp
{
    [[vk::location(1)]] uint instID;
};


struct GPU_DepthVsOutput
{
    GPU_DepthVsOutputInterp data;
    nointerpolation GPU_DepthVsOutputNoInterp flatData;

    float4 hpos : SV_Position; 
};

#endif