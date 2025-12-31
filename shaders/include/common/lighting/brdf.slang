#ifndef BRDF_FX
#define BRDF_FX

#include "common/math/math.slang"


float DistributionGGX(in float3 N, in float3 H, in float a)
{
    const float a2     = a * a;
    const float NdotH  = max(dot(N, H), 0.f);
    const float NdotH2 = NdotH * NdotH;
	
    float denom  = (NdotH2 * (a2 - 1.f) + 1.f);
    denom        = M3D_PI * denom * denom;

    return a2 / denom;
}


float GeometrySchlickGGX(in float NdotV, in float k)
{
    const float denom = NdotV * (1.f - k) + k;

    return NdotV / denom;
}


float GeometrySmith(in float3 N, in float3 V, in float3 L, in float k)
{
    const float NdotV = max(dot(N, V), 0.f);
    const float NdotL = max(dot(N, L), 0.f);
    const float ggx1 = GeometrySchlickGGX(NdotV, k);
    const float ggx2 = GeometrySchlickGGX(NdotL, k);
	
    return ggx1 * ggx2;
}


float3 FresnelSchlick(in float cosTheta, in float3 F0)
{
    return F0 + (1.f - F0) * pow(1.f - cosTheta, 5.f);
}

#endif