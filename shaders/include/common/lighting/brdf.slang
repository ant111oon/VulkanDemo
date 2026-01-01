#ifndef BRDF_FX
#define BRDF_FX

#include "common/math/math.slang"


float DistributionGGX(in float NdotH, in float roughness)
{
    const float a2 = roughness * roughness;
    const float NdotH2 = NdotH * NdotH;
	
    float denom = (NdotH2 * (a2 - 1.f) + 1.f);
    denom = M3D_PI * denom * denom;

    return a2 / max(denom, M3D_EPS);
}


float GeometrySchlickGGX(in float NdotV, in float k)
{
    const float denom = NdotV * (1.f - k) + k;

    return NdotV / max(denom, M3D_EPS);
}


float GeometrySmith(in float NdotV, in float NdotL, in float k)
{
    const float ggx1 = GeometrySchlickGGX(NdotV, k);
    const float ggx2 = GeometrySchlickGGX(NdotL, k);
	
    return ggx1 * ggx2;
}


float3 FresnelSchlick(in float cosTheta, in float3 F0)
{
    return F0 + (1.f - F0) * pow(saturate(1.f - cosTheta), 5.f);
}

#endif