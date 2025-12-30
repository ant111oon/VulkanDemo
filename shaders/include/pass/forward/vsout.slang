#ifndef FORWARD_VS_OUT_H
#define FORWARD_VS_OUT_H


struct FORWARD_VS_OUTPUT_INTERP
{
    [[vk::location(0)]] float3 lpos;
    [[vk::location(1)]] float3 wpos;
    [[vk::location(2)]] float3 lnorm;
    [[vk::location(3)]] float2 uv;
    [[vk::location(4)]] float4 tangent;
};


struct FORWARD_VS_OUTPUT_NO_INTERP
{
    [[vk::location(5)]] uint instID;
};


struct FORWARD_VS_OUTPUT
{
    FORWARD_VS_OUTPUT_INTERP data;
    nointerpolation FORWARD_VS_OUTPUT_NO_INTERP flatData;

    float4 hpos : SV_Position; 
};

#endif