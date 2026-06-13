#ifndef DBG_RT_VIEW_VS_OUT_H
#define DBG_RT_VIEW_VS_OUT_H


struct DBG_RT_VIEW_VS_OUTPUT_INTERP
{
};


struct DBG_RT_VIEW_VS_OUTPUT_NO_INTERP
{
};


struct DBG_RT_VIEW_VS_OUTPUT
{
    DBG_RT_VIEW_VS_OUTPUT_INTERP data;
    nointerpolation DBG_RT_VIEW_VS_OUTPUT_NO_INTERP flatData;

    float4 hpos : SV_Position;
};

#endif