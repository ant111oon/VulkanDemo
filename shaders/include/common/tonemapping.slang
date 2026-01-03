#ifndef TONEMAPPING_H
#define TONEMAPPING_H


float3 Reinhard_Tonemap(in float3 color)
{
    return color / (color + 1.f);
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

#endif