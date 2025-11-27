#ifndef BASE_VS_OUT_H
#define BASE_VS_OUT_H

struct BASE_VS_OUTPUT_INTERP
{
    [[vk::location(0)]] float3 lnorm;
    [[vk::location(1)]] float2 texCoord;
};


struct BASE_VS_OUTPUT_NO_INTERP
{
    [[vk::location(2)]] uint instID;
};


struct BASE_VS_OUTPUT
{
    BASE_VS_OUTPUT_INTERP data;
    nointerpolation BASE_VS_OUTPUT_NO_INTERP flatData;

    float4 hpos : SV_Position; 
};

#endif