#ifndef POST_PROCESSING_VS_OUT_H
#define POST_PROCESSING_VS_OUT_H


struct GPU_PostProcVsOutputInterp
{
};


struct GPU_PostProcVsOutputNoInterp
{
};


struct GPU_PostProcVsOutput
{
    GPU_PostProcVsOutputInterp data;
    nointerpolation GPU_PostProcVsOutputNoInterp flatData;

    float4 hpos : SV_Position;
};

#endif