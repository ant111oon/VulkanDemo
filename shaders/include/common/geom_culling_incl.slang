#ifndef GEOM_CULLING_INCL_H
#define GEOM_CULLING_INCL_H

// NOTE:
//  - GEOM_CULLING_PHASE_1/GEOM_CULLING_PHASE_2 (mandatory)
//  - GEOM_CULLING_FRUSTUM_ENABLED      enables frustum culling
//  - GEOM_CULLING_CONTRIBUTION_ENABLED enables pixel contribution culling
//  - GEOM_CULLING_HZB_ENABLED          enables HZB culling


#if !defined(GEOM_CULLING_PHASE_1) && !defined(GEOM_CULLING_PHASE_2)
    #error One of GEOM_CULLING_PHASE_1 or GEOM_CULLING_PHASE_2 macro must be defined
#else
    #if defined(GEOM_CULLING_PHASE_2)
        #define GEOM_CULLING_WRITE_GLOBAL_DATA
    #endif
#endif

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
#define HZB_CULLING_HZB_MIPS_COUNT  GEOM_CULLING_HZB_MIPS_COUNT
#include "common/math/hzb_culling.slang"

#include "common/math/contribution_culling.slang"


#ifndef ENV_RELEASE
    #define DBG_IS_GEOM_GPU_FRUSTUM_CULLING_ENABLED         COMMON_DBG_IS_GEOM_GPU_FRUSTUM_CULLING_ENABLED
    #define DBG_IS_GEOM_GPU_CONTRIBUTION_CULLING_ENABLED    COMMON_DBG_IS_GEOM_GPU_CONTRIBUTION_CULLING_ENABLED
    #define DBG_IS_GEOM_GPU_HZB_CULLING_ENABLED             COMMON_DBG_IS_GEOM_GPU_HZB_CULLING_ENABLED
#else
    #define DBG_IS_GEOM_GPU_FRUSTUM_CULLING_ENABLED         true
    #define DBG_IS_GEOM_GPU_CONTRIBUTION_CULLING_ENABLED    true
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


    void GeomCullingWriteDrawCmd(RWStructuredBuffer<uint> buffer, in uint index, in COMMON_CMD_DRAW_INDEXED_INDIRECT cmd)
    {
        static const uint CMD_SIZE_UINT = sizeof(cmd) / sizeof(uint);

        // First uint in the buffer is counter
        const uint indexUInt = 1 + index * CMD_SIZE_UINT;
    
        buffer[indexUInt + 0] = cmd.INDEX_COUNT;
        buffer[indexUInt + 1] = cmd.INSTANCE_COUNT;
        buffer[indexUInt + 2] = cmd.FIRST_INDEX;
        buffer[indexUInt + 3] = cmd.FIRST_VERTEX;
        buffer[indexUInt + 4] = cmd.FIRST_INSTANCE;
    }


    void GeomCullingPushInstToCmdQueue(RWStructuredBuffer<uint> drawCmdBuffer, RWStructuredBuffer<uint> visInstIDBuffer, in COMMON_CMD_DRAW_INDEXED_INDIRECT cmd, in uint visInstID)
    {
        uint index = 0;

        // First UINT of draw cmd buffer contains counter
        InterlockedAdd(drawCmdBuffer[0], 1, index);

        GeomCullingWriteDrawCmd(drawCmdBuffer, index, cmd);
        visInstIDBuffer[index] = visInstID;
    }


    // temp hack solution
    uint GeomCullingGetLodIndex(in COMMON_MESH mesh, in float dist)
    {
        static const float LOD_DIST = 20.f;

        uint lodID = 0;
        while (lodID < mesh.LOD_COUNT && lodID * LOD_DIST < dist) {
            ++lodID;
        }
        
        return clamp(COMMON_CB.DBG_FORCED_GEOM_LOD >= 0 ? COMMON_CB.DBG_FORCED_GEOM_LOD : lodID - 1, 0, mesh.LOD_COUNT - 1);
    }
}


bool GeomCullingVisibility(in AABB aabb)
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
        if (!Contribution_Visibility(sbox, COMMON_SCREEN_SIZE, FLOAT2(GEOM_CULLING_VIS_CONTRIBUTION_FALLOFF))) {
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


bool GeomCullingTestInstanceVisibilityFlag(in uint instID)
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


uint GeomCullingGetQueueIndex(in COMMON_MATERIAL material)
{
    if (material.IsTransparent()) {
        return (uint)GEOM_CULLING_QUEUE_ID::TRANSPARENT;
    } else if (material.IsAlphaKill()) {
        return (uint)GEOM_CULLING_QUEUE_ID::AKILL;
    } else {
        return (uint)GEOM_CULLING_QUEUE_ID::OPAQUE;
    }
}


void GeomCullingPushInstToCmdQueue(in uint instID, in COMMON_INST inst, in float dist)
{
    const COMMON_MESH mesh = COMMON_MESH_BUFFER[inst.MESH_IDX];

    const uint lodID = utils::GeomCullingGetLodIndex(mesh, dist);
    const COMMON_MESH_LOD LOD = COMMON_MESH_LOD_BUFFER[mesh.FIRST_LOD + lodID];

    COMMON_CMD_DRAW_INDEXED_INDIRECT cmd = COMMON_CMD_DRAW_INDEXED_INDIRECT(
        LOD.INDEX_COUNT, 1, LOD.FIRST_INDEX, mesh.FIRST_VERTEX, instID
    );

    const uint queueID = GeomCullingGetQueueIndex(COMMON_MATERIALS[inst.MATERIAL_IDX]);

    utils::GeomCullingPushInstToCmdQueue(GEOM_CULL_DRAW_CMDS_UAV[queueID], GEOM_CULL_INST_INFO_IDS_UAV[queueID], cmd, instID);

    // NOTE: A little bit complicated logic but that's how it is:
    //       GLOBAL and LOCAL (ocluders pass) buffers are the same memory so for that pass we write only in LOCAL buffers
    //       For occludees pass we write in GLOBAL and LOCAL buffers to use GLOBAL data after depth pass in GBUFFER pass

#if defined(GEOM_CULLING_WRITE_GLOBAL_DATA)
    utils::GeomCullingPushInstToCmdQueue(GEOM_CULL_GLOBAL_DRAW_CMDS_UAV[queueID], GEOM_CULL_GLOBAL_INST_INFO_IDS_UAV[queueID], cmd, instID);
#endif
}


#endif