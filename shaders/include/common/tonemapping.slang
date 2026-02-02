#ifndef TONEMAPPING_H
#define TONEMAPPING_H

#include "common/math/constants.slang"


float3 Reinhard_Tonemap(in float3 color)
{
    float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float mappedLuminance = (luminance * (1.f + luminance)) / (1.f + luminance);

    return (mappedLuminance / luminance) * color;
}


float3 ACES_Tonemap(in float3 color)
{
    const float A = 2.51f;
    const float B = 0.03f;
    const float C = 2.43f;
    const float D = 0.59f;
    const float E = 0.14f;
    
    return saturate((color * (A * color + B)) / (color * (C * color + D) + E));
}


float3 PartialUncharted2_Tonemap(in float3 color)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;

    return ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
}


float3 Uncharted2_Tonemap(in float3 color)
{
    const float exposureBias = 2.f;
    const float3 curr = PartialUncharted2_Tonemap(color * exposureBias);

    float3 W = FLOAT3(11.2f);
    const float3 whiteScale = ONEF3 / PartialUncharted2_Tonemap(W);
    return curr * whiteScale;
}

#endif