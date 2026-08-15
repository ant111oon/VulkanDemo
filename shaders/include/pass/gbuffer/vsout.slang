#ifndef GBUFFER_VS_OUT_H
#define GBUFFER_VS_OUT_H


struct GPU_GBufferVsOutputInterp
{
    [[vk::location(0)]] float3 lpos;
    [[vk::location(1)]] float3 wpos;
    [[vk::location(2)]] float3 lnorm;
    [[vk::location(3)]] float2 uv;
    [[vk::location(4)]] float4 tangent;
};


struct GPU_GBufferVsOutputNoInterp
{
    [[vk::location(5)]] uint instID;
};


struct GPU_GBufferVsOutput
{
    GPU_GBufferVsOutputInterp data;
    nointerpolation GPU_GBufferVsOutputNoInterp flatData;

    float4 hpos : SV_Position; 
};

#endif