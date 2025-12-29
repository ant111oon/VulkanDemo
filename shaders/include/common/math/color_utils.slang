#ifndef COLOR_UTILS_H
#define COLOR_UTILS_H

#include "math.slang"

float SRGBToLinear(in float color)
{
    // https://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
    return color * (color * (color * 0.305306011 + 0.682171111) + 0.012522878);
}


float3 SRGBToLinear(in float3 color)
{
    return float3(SRGBToLinear(color.r), SRGBToLinear(color.g), SRGBToLinear(color.b)); 
}


float LinearToSRGB(in float color)
{
    return color < 0.0031308f ? 12.92f * color : 1.055f * pow(color, 1.f / 2.4f) - 0.055f;
}


float3 LinearToSRGB(in float3 color)
{
    return float3(LinearToSRGB(color.r), LinearToSRGB(color.g), LinearToSRGB(color.b));
}


float3 LinearToSRGBFast(in float3 color)
{
    return pow(saturate(color), 1.f / 2.2f);
}

#endif