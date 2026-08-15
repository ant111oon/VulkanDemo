#ifndef CSM_VS_OUT_H
#define CSM_VS_OUT_H


struct GPU_CsmOutputInterp
{
    [[vk::location(0)]] float2 uv;
};


struct GPU_CsmOutputNoInterp
{
    [[vk::location(1)]] uint instID;
};


struct GPU_CsmVsOutput
{
    GPU_CsmOutputInterp data;
    nointerpolation GPU_CsmOutputNoInterp flatData;

    float4 hpos : SV_Position; 
};

#endif