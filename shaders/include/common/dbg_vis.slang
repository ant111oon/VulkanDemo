#ifndef DBG_VIS_H
#define DBG_VIS_H

#include "registers/common_registers.slang"


void DbgVisOutput(out float4 outColor, in float4 albedo, in float3 wnorm, in float metalness, in float roughness, in float ao, in float3 emissive, in float3 vNorm, in float4 vTang)
{
    if (COMMON_DBG_VIS_GBUFFER_ALBEDO) {
        outColor = albedo;
    } else if (COMMON_DBG_VIS_GBUFFER_NORMAL) {
        outColor = float4(wnorm, 1.f);
    } else if (COMMON_DBG_VIS_GBUFFER_METALNESS_MASK) {
        outColor = float4(FLOAT3(metalness), 1.f);
    } else if (COMMON_DBG_VIS_GBUFFER_ROUGHNESS) {
        outColor = float4(FLOAT3(roughness), 1.f);
    } else if (COMMON_DBG_VIS_GBUFFER_AO) {
        outColor = float4(FLOAT3(ao), 1.f);
    } else if (COMMON_DBG_VIS_GBUFFER_EMISSIVE) {
        outColor = float4(emissive, 1.f);
    } else if (COMMON_DBG_VIS_VERT_NORMAL) {
        outColor = float4(vNorm * 0.5f + 0.5f, 1.f);
    } else if (COMMON_DBG_VIS_VERT_TANGENT) {
        outColor = float4(vTang.xyz * sign(vTang.w) * 0.5f + 0.5f, 1.f);
    }
}

#endif