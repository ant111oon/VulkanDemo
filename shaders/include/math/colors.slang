#ifndef COLOR_CONVERSION_H
#define COLOR_CONVERSION_H

#include "math.slang"

// https://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
float3 SRGBToLinear(in float3 sRGB)
{
    return sRGB * (sRGB * (sRGB * 0.305306011 + 0.682171111) + 0.012522878);
}

#endif