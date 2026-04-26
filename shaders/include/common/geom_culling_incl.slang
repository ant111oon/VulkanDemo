#ifndef GEOM_CULLING_INCL_H
#define GEOM_CULLING_INCL_H

// NOTE:
//  - GEOM_CULLING_FRUSTUM_ENABLED enables frustum culling
//  - GEOM_CULLING_CONTRIBUTION_ENABLED enables pixel contribution culling
//  - GEOM_CULLING_HZB_ENABLED enables HZB culling

#if defined(GEOM_CULLING_FRUSTUM_ENABLED) || defined(GEOM_CULLING_CONTRIBUTION_ENABLED) || defined(GEOM_CULLING_HZB_ENABLED)
    #define GEOM_CULLING_ENABLED
#endif


#include "registers/common_registers.slang"
#include "registers/geom_culling_registers.slang"

#define FRUSTUM_CULLING_VIEW_PROJ_MATR COMMON_CB.CULLING_VIEW_PROJ_MATRIX
#include "common/math/frustum_culling.slang"

#define HZB_CULLING_HZB_TEX_SAMPLER GET_COMMON_SAMPLER(COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE)
#define HZB_CULLING_HZB_TEX         COMMON_HZB
#define HZB_CULLING_HZB_MIP_0_SIZE  COMMON_SCREEN_SIZE
#define HZB_CULLING_VIEW_PROJ_MATR  FRUSTUM_CULLING_VIEW_PROJ_MATR
#define HZB_CULLING_HZB_MIPS_COUNT  MESH_CULLING_HZB_MIPS_COUNT
#include "common/math/hzb_culling.slang"

#include "common/math/contribution_culling.slang"


#ifndef ENV_RELEASE
    #define DBG_IS_GEOM_GPU_FRUSTUM_CULLING_ENABLED         COMMON_DBG_IS_MESH_GPU_FRUSTUM_CULLING_ENABLED
    #define DBG_IS_GEOM_GPU_CONTRIBUTION_CULLING_ENABLED    COMMON_DBG_IS_MESH_GPU_CONTRIBUTION_CULLING_ENABLED
    #define DBG_IS_GEOM_GPU_HZB_CULLING_ENABLED             COMMON_DBG_IS_MESH_GPU_HZB_CULLING_ENABLED
#else
    #define DBG_IS_GEOM_GPU_FRUSTUM_CULLING_ENABLED         true
    #define DBG_IS_GEOM_GPU_CONTRIBUTION_CULLING_ENABLED    true
    #define DBG_IS_GEOM_GPU_HZB_CULLING_ENABLED             true
#endif


bool Geom_Visibility(in COMMON_INST_DATA inst)
{
#if !defined(GEOM_CULLING_ENABLED)
    return true;
#endif

    // Runtime debug flags
    const bool isFrustumCullEnabled = DBG_IS_GEOM_GPU_FRUSTUM_CULLING_ENABLED;
    const bool isContribCullEnabled = DBG_IS_GEOM_GPU_CONTRIBUTION_CULLING_ENABLED;
    const bool isHZBCullEnabled     = DBG_IS_GEOM_GPU_HZB_CULLING_ENABLED;
    const bool isCullEnabled = isFrustumCullEnabled || isContribCullEnabled || isHZBCullEnabled;

    if (!isCullEnabled) {
        return true;
    }

    AABB aabb = COMMON_AABB_BUFFER[inst.VOLUME_IDX].GetAABB();

#if defined(GEOM_CULLING_FRUSTUM_ENABLED)
    if (isFrustumCullEnabled) {
        if (!Frustum_Visibility(COMMON_CULLING_CAMERA_FRUSTUM, aabb)) {
            return false;
        }
    }
#endif

#if defined(GEOM_CULLING_CONTRIBUTION_ENABLED) || defined(GEOM_CULLING_HZB_ENABLED)
    if (!isContribCullEnabled && !isHZBCullEnabled) {
        return true;
    }

    SBOX sbox = aabb.GetSBOX(HZB_CULLING_VIEW_PROJ_MATR);

    sbox.minUV = saturate(sbox.minUV);
    sbox.maxUV = saturate(sbox.maxUV);
    sbox.closestZ = saturate(sbox.closestZ);
#endif

#if defined(GEOM_CULLING_CONTRIBUTION_ENABLED)
    if (isContribCullEnabled) {
        if (!Contribution_Visibility(sbox, COMMON_SCREEN_SIZE, FLOAT2(MESH_CULLING_VIS_CONTRIBUTION_FALLOFF))) {
            return false;
        }
    }
#endif

#if defined(GEOM_CULLING_HZB_ENABLED)
    if (isHZBCullEnabled) {
        if (!HZB_Visibility(sbox)) {
            return false;
        }
    }
#endif
    
    return true; 
}


#endif