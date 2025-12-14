#ifndef DBG_VIS_H
#define DBG_VIS_H

#include "registers/common_registers.slang"


void DbgVisOutput(out float4 outColor, in float4 albedo, in float3 normal, in float metalness, in float roughness, in float ao, in float4 emissive, in float3 vertNormal)
{
    if (COMMON_DBG_VIS_GBUFFER_ALBEDO) {
        outColor = albedo;
    } else if (COMMON_DBG_VIS_GBUFFER_NORMAL) {
        outColor = float4(normal, 1.f);
    } else if (COMMON_DBG_VIS_GBUFFER_METALNESS_MASK) {
        outColor = float4(FLOAT3(metalness), 1.f);
    } else if (COMMON_DBG_VIS_GBUFFER_ROUGHNESS) {
        outColor = float4(FLOAT3(roughness), 1.f);
    } else if (COMMON_DBG_VIS_GBUFFER_AO) {
        outColor = float4(FLOAT3(ao), 1.f);
    } else if (COMMON_DBG_VIS_GBUFFER_EMISSIVE) {
        outColor = emissive;
    } else if (COMMON_DBG_VIS_VERT_NORMAL) {
        outColor = float4(vertNormal * 0.5f + 0.5f, 1.f);
    }
}

#endif