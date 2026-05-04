#ifndef DBG_VIS_H
#define DBG_VIS_H

#ifdef ENV_DEBUG
    #define DBG_RT_OUTPUT_ENABLED
#endif

#include "registers/common_registers.slang"


#ifdef DBG_RT_OUTPUT_ENABLED
    #define COMMON_DBG_VIS_NONE                   IS_FLAG_SET(COMMON_CB.DBG_VIS_FLAGS, COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_NONE_MASK)
    #define COMMON_DBG_VIS_GBUFFER_ALBEDO         IS_FLAG_SET(COMMON_CB.DBG_VIS_FLAGS, COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_ALBEDO_MASK)
    #define COMMON_DBG_VIS_GBUFFER_NORMAL         IS_FLAG_SET(COMMON_CB.DBG_VIS_FLAGS, COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_NORMAL_MASK)
    #define COMMON_DBG_VIS_GBUFFER_METALNESS_MASK IS_FLAG_SET(COMMON_CB.DBG_VIS_FLAGS, COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_METALNESS_MASK)
    #define COMMON_DBG_VIS_GBUFFER_ROUGHNESS      IS_FLAG_SET(COMMON_CB.DBG_VIS_FLAGS, COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_ROUGHNESS_MASK)
    #define COMMON_DBG_VIS_GBUFFER_AO             IS_FLAG_SET(COMMON_CB.DBG_VIS_FLAGS, COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_AO_MASK)
    #define COMMON_DBG_VIS_GBUFFER_EMISSIVE       IS_FLAG_SET(COMMON_CB.DBG_VIS_FLAGS, COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_GBUFFER_EMISSIVE_MASK)
    #define COMMON_DBG_VIS_VERT_NORMAL            IS_FLAG_SET(COMMON_CB.DBG_VIS_FLAGS, COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_VERT_NORMAL_MASK)
    #define COMMON_DBG_VIS_VERT_TANGENT           IS_FLAG_SET(COMMON_CB.DBG_VIS_FLAGS, COMMON_DBG_VIS_FLAG_MASKS::DBG_VIS_VERT_TANGENT_MASK)


    void DbgRTVisOutput(out float4 outColor, in float4 albedo, in float3 wnorm, in float metalness, in float roughness, in float ao, in float3 emissive, in float3 vNorm, in float4 vTang)
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
#else
    #define COMMON_DBG_VIS_NONE                   true
    #define COMMON_DBG_VIS_GBUFFER_ALBEDO         false
    #define COMMON_DBG_VIS_GBUFFER_NORMAL         false
    #define COMMON_DBG_VIS_GBUFFER_METALNESS_MASK false
    #define COMMON_DBG_VIS_GBUFFER_ROUGHNESS      false
    #define COMMON_DBG_VIS_GBUFFER_AO             false
    #define COMMON_DBG_VIS_GBUFFER_EMISSIVE       false
    #define COMMON_DBG_VIS_VERT_NORMAL            false
    #define COMMON_DBG_VIS_VERT_TANGENT           false
#endif

#endif