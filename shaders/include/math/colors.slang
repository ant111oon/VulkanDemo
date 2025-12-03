#ifndef COLOR_CONVERSION_H
#define COLOR_CONVERSION_H

#include "math.slang"

// https://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
float3 SRGBToLinear(in float3 sRGB)
{
    return sRGB * (sRGB * (sRGB * 0.305306011 + 0.682171111) + 0.012522878);
}


float3 LinearToSRGB(in float3 rgb)
{
    const float3 S1 = sqrt(rgb);
    const float3 S2 = sqrt(S1);
    const float3 S3 = sqrt(S2);

    return 0.585122381 * S1 + 0.783140355 * S2 - 0.368262736 * S3;
}

#endif