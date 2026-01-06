#ifndef SKYBOX_VS_OUT_H
#define SKYBOX_VS_OUT_H


struct SKYBOX_VS_OUTPUT_INTERP
{
    [[vk::location(0)]] float3 uvw;
};


struct SKYBOX_VS_OUTPUT_NO_INTERP
{
};


struct SKYBOX_VS_OUTPUT
{
    SKYBOX_VS_OUTPUT_INTERP data;
    nointerpolation SKYBOX_VS_OUTPUT_NO_INTERP flatData;

    float4 hpos : SV_Position; 
};

#endif