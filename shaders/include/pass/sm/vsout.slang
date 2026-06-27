#ifndef SM_VS_OUT_H
#define SM_VS_OUT_H


struct SM_VS_OUTPUT_INTERP
{
    [[vk::location(0)]] float2 uv;
};


struct SM_VS_OUTPUT_NO_INTERP
{
    [[vk::location(1)]] uint instID;
};


struct SM_VS_OUTPUT
{
    SM_VS_OUTPUT_INTERP data;
    nointerpolation SM_VS_OUTPUT_NO_INTERP flatData;

    float4 hpos : SV_Position; 
};

#endif