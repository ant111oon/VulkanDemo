#ifndef GEOM_CULLING_INCL_H
#define GEOM_CULLING_INCL_H

// NOTE:
//  - GEOM_CULLING_PHASE_1/GEOM_CULLING_PHASE_2 (mandatory)
//  - GEOM_CULLING_FRUSTUM_ENABLED      enables frustum culling
//  - GEOM_CULLING_HZB_ENABLED          enables HZB culling


#if !defined(GEOM_CULLING_PHASE_1) && !defined(GEOM_CULLING_PHASE_2)
    #error One of GEOM_CULLING_PHASE_1 or GEOM_CULLING_PHASE_2 macro must be defined
#endif

#if defined(GEOM_CULLING_FRUSTUM_ENABLED) || defined(GEOM_CULLING_HZB_ENABLED)
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
#define HZB_CULLING_HZB_MIPS_COUNT  GEOM_CULLING_HZB_MIPS_COUNT
#include "common/math/hzb_culling.slang"


#ifndef ENV_RELEASE
    #define DBG_IS_GEOM_GPU_FRUSTUM_CULLING_ENABLED         COMMON_DBG_IS_GEOM_GPU_FRUSTUM_CULLING_ENABLED
    #define DBG_IS_GEOM_GPU_HZB_CULLING_ENABLED             COMMON_DBG_IS_GEOM_GPU_HZB_CULLING_ENABLED
#else
    #define DBG_IS_GEOM_GPU_FRUSTUM_CULLING_ENABLED         true
    #define DBG_IS_GEOM_GPU_HZB_CULLING_ENABLED             true
#endif


namespace utils
{
    void GeomCullingGetInstVisBitBatchAndLocalID(in uint instID, out uint batchID, out uint localID)
    {
        static const uint VIS_FLAGS_BATCH_SIZE_BITS = sizeof(uint) * 8;

        batchID = instID / (VIS_FLAGS_BATCH_SIZE_BITS);
        localID = instID % (VIS_FLAGS_BATCH_SIZE_BITS);
    }
}


bool GeomCullingVisibility(in AABB aabb)
{
#if !defined(GEOM_CULLING_ENABLED)
    return true;
#endif

    // Runtime debug flags
    const bool isFrustumCullEnabled = DBG_IS_GEOM_GPU_FRUSTUM_CULLING_ENABLED;
    const bool isHZBCullEnabled     = DBG_IS_GEOM_GPU_HZB_CULLING_ENABLED;

    if (!isFrustumCullEnabled && !isHZBCullEnabled) {
        return true;
    }

#if defined(GEOM_CULLING_FRUSTUM_ENABLED)
    if (isFrustumCullEnabled) {
        if (!Frustum_Visibility(COMMON_CULLING_CAMERA_FRUSTUM, aabb)) {
            return false;
        }
    }
#endif

#if defined(GEOM_CULLING_HZB_ENABLED)
    if (!isHZBCullEnabled) {
        return true;
    }

    SBOX sbox = aabb.GetSBOX(HZB_CULLING_VIEW_PROJ_MATR);

    sbox.minUV = saturate(sbox.minUV);
    sbox.maxUV = saturate(sbox.maxUV);
    sbox.closestZ = saturate(sbox.closestZ);
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


bool GeomCullingTestInstVisFlag(in uint instID)
{
    uint batchID, localID;
    utils::GeomCullingGetInstVisBitBatchAndLocalID(instID, batchID, localID);

    const uint mask = 1u << localID;

    return IS_FLAG_SET(GEOM_CULL_VIS_FLAGS_UAV[batchID], mask);
}


void GeomCullingWriteVisFlag(in uint instID, bool isVisible)
{
    uint batchID, localID;
    utils::GeomCullingGetInstVisBitBatchAndLocalID(instID, batchID, localID);

    const uint mask = 1u << localID;

    if (isVisible) {
        InterlockedOr(GEOM_CULL_VIS_FLAGS_UAV[batchID], mask);
    } else {
        InterlockedAnd(GEOM_CULL_VIS_FLAGS_UAV[batchID], ~mask);
    }
}


uint GeomCullingGetQueueMask(in COMMON_INST inst)
{
    uint mask = 0;

    const COMMON_MATERIAL material = COMMON_MATERIALS[inst.MATERIAL_IDX];

    if (material.IsAlphaKill()) {
        mask |= (1u << GEOM_QUEUE_AKILL);
    } else {
        mask |= (1u << GEOM_QUEUE_OPAQUE);
    }

    return mask;
}


#endif