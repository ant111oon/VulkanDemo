#ifndef DBG_DRAW_VS_OUT_H
#define DBG_DRAW_VS_OUT_H


struct GPU_DbgDrawVsOutputInterp
{
    [[vk::location(0)]] float4 color;
};


struct GPU_DbgDrawVsOutputNoInterp
{
};


struct GPU_DbgDrawVsOutput
{
    GPU_DbgDrawVsOutputInterp data;
    nointerpolation GPU_DbgDrawVsOutputNoInterp flatData;

    float4 hpos : SV_Position; 
};

#endif