#ifndef DBG_RT_VIEW_VS_OUT_H
#define DBG_RT_VIEW_VS_OUT_H


struct GPU_DbgRTViewVsOutputInterp
{
};


struct GPU_DbgRTViewVsOutputNoInterp
{
};


struct GPU_DbgRTViewVsOutput
{
    GPU_DbgRTViewVsOutputInterp data;
    nointerpolation GPU_DbgRTViewVsOutputNoInterp flatData;

    float4 hpos : SV_Position;
};

#endif