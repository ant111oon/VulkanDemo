#ifndef CSM_VS_OUT_H
#define CSM_VS_OUT_H


struct CSM_VS_OUTPUT_INTERP
{
    [[vk::location(0)]] float2 uv;
};


struct CSM_VS_OUTPUT_NO_INTERP
{
    [[vk::location(1)]] uint instID;
};


struct CSM_VS_OUTPUT
{
    CSM_VS_OUTPUT_INTERP data;
    nointerpolation CSM_VS_OUTPUT_NO_INTERP flatData;

    float4 hpos : SV_Position; 
};

#endif