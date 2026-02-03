#ifndef ZPASS_VS_OUT_H
#define ZPASS_VS_OUT_H


struct ZPASS_VS_OUTPUT_INTERP
{
    [[vk::location(0)]] float2 uv;
};


struct ZPASS_VS_OUTPUT_NO_INTERP
{
    [[vk::location(1)]] uint instID;
};


struct ZPASS_VS_OUTPUT
{
    ZPASS_VS_OUTPUT_INTERP data;
    nointerpolation ZPASS_VS_OUTPUT_NO_INTERP flatData;

    float4 hpos : SV_Position; 
};

#endif