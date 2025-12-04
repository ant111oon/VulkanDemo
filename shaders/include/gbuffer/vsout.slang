#ifndef GBUFFER_VS_OUT_H
#define GBUFFER_VS_OUT_H


struct GBUFFER_VS_OUTPUT_INTERP
{
    [[vk::location(0)]] float3 lnorm;
    [[vk::location(1)]] float2 texCoord;
};


struct GBUFFER_VS_OUTPUT_NO_INTERP
{
    [[vk::location(2)]] uint instID;
};


struct GBUFFER_VS_OUTPUT
{
    GBUFFER_VS_OUTPUT_INTERP data;
    nointerpolation GBUFFER_VS_OUTPUT_NO_INTERP flatData;

    float4 hpos : SV_Position; 
};

#endif