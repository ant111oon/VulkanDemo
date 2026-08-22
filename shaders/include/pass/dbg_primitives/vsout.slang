#ifndef DBG_DRAW_VS_OUT_H
#define DBG_DRAW_VS_OUT_H


struct GPU_DbgPrimVsOutputInterp
{
    [[vk::location(0)]] float4 color;
};


struct GPU_DbgPrimVsOutputNoInterp
{
};


struct GPU_DbgPrimVsOutput
{
    GPU_DbgPrimVsOutputInterp data;
    nointerpolation GPU_DbgPrimVsOutputNoInterp flatData;

    float4 hpos : SV_Position; 
};

#endif