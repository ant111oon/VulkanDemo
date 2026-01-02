#ifndef BRDF_FX
#define BRDF_FX

#include "common/math/math.slang"


float NDF_GGX(in float NdotH, in float roughness)
{
    const float a2 = roughness * roughness;
    const float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.f) + 1.f);
    denom = max(M3D_PI * denom * denom, M3D_EPS);

    return a2 / denom;
}


float G_GGX(in float NdotV, in float k)
{
    const float denom = max(NdotV * (1.f - k) + k, M3D_EPS);

    return NdotV / denom;
}


float G_Smith(in float NdotV, in float NdotL, in float k)
{
    return G_GGX(NdotV, k) * G_GGX(NdotL, k);
}


float3 F_Schlick(in float VdotH, in float3 F0)
{
    return F0 + (1.f - F0) * pow(saturate(1.f - VdotH), 5.f);
}

#endif