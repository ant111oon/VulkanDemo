#ifndef BRDF_FX
#define BRDF_FX

#include "common/math/utils.slang"


static const float3 COMMON_DIELECTRIC_F0 = FLOAT3(0.04f);


float3 CalcDiffuseCoef(in float3 specularCoef, in float metalness)
{
    return (ONEF3 - specularCoef) * (1.f - metalness);
}


float CalcDirectK(in float roughness)
{
    const float k = roughness + 1.f;
    return (k * k) / 8.f;
}


float CalcIndirectK(in float roughness)
{
    return (roughness * roughness) * 0.5f;
}


float NDF_GGX(in float NdotH, in float roughness)
{
    const float a = roughness * roughness;
    const float a2 = a * a;
    const float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.f) + 1.f);
    denom = M3D_PI * denom * denom;

    return a2 / max(denom, M3D_EPS);
}


float G_GGX(in float NdotV, in float k)
{
    return NdotV / max(NdotV * (1.f - k) + k, M3D_EPS);
}


float G_Smith(in float NdotV, in float NdotL, in float k)
{
    return G_GGX(NdotV, k) * G_GGX(NdotL, k);
}


float3 F_Schlick(in float VdotH, in float3 F0)
{
    return F0 + (1.f - F0) * pow(saturate(1.f - VdotH), 5.f);
}


float3 F_SchlickRoughness(in float VdotH, in float3 F0, float roughness)
{
    return F0 + (max(float3(1.f - roughness), F0) - F0) * pow(saturate(1.f - VdotH), 5.f);
}


float3 ImportanceSampleGGX(in float2 Xi, in float roughness, in float3 tang, in float3 bitang, in float3 normal)
{
    const float a = roughness * roughness;
	
    const float phi = M3D_2_PI * Xi.x;
    const float cosTheta = sqrt((1.f - Xi.y) / (1.f + (a * a - 1.f) * Xi.y));
    const float sinTheta = sqrt(1.f - cosTheta * cosTheta);

    float sinPhi, cosPhi;
    sincos(phi, sinPhi, cosPhi);

    // from spherical coordinates to cartesian coordinates
    float3 H;
    H.x = cosPhi * sinTheta;
    H.y = sinPhi * sinTheta;
    H.z = cosTheta;

    return normalize(tang * H.x + bitang * H.y + normal * H.z);
}

#endif