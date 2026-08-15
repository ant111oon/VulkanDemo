#ifndef SKYBOX_VS_OUT_H
#define SKYBOX_VS_OUT_H


struct GPU_SkyboxVsOutputInterp
{
    [[vk::location(0)]] float3 uvw;
};


struct GPU_SkyboxVsOutputNoInterp
{
};


struct GPU_SkyboxVsOutput
{
    GPU_SkyboxVsOutputInterp data;
    nointerpolation GPU_SkyboxVsOutputNoInterp flatData;

    float4 hpos : SV_Position; 
};

#endif