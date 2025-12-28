#ifndef POST_PROCESSING_VS_OUT_H
#define POST_PROCESSING_VS_OUT_H


struct POST_PROC_VS_OUTPUT_INTERP
{
};


struct POST_PROC_VS_OUTPUT_NO_INTERP
{
};


struct POST_PROC_VS_OUTPUT
{
    POST_PROC_VS_OUTPUT_INTERP data;
    nointerpolation POST_PROC_VS_OUTPUT_NO_INTERP flatData;

    float4 hpos : SV_Position;
};

#endif