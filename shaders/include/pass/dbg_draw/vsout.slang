#ifndef DBG_DRAW_VS_OUT_H
#define DBG_DRAW_VS_OUT_H


struct DBG_DRAW_VS_OUTPUT_INTERP
{
    [[vk::location(0)]] float4 color;
};


struct DBG_DRAW_VS_OUTPUT_NO_INTERP
{
};


struct DBG_DRAW_VS_OUTPUT
{
    DBG_DRAW_VS_OUTPUT_INTERP data;
    nointerpolation DBG_DRAW_VS_OUTPUT_NO_INTERP flatData;

    float4 hpos : SV_Position; 
};

#endif