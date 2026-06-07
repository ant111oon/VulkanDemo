#ifndef GEOM_CULLING_INCL_H
#define GEOM_CULLING_INCL_H


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


enum GEOM_CULLING_PHASE
{
    PHASE_1,
    PHASE_2
}


groupshared uint GROUP_VIS_ID_QUEUE_SIZES[GEOM_QUEUE_COUNT];
groupshared uint GROUP_VIS_ID_QUEUE_OFFSETS[GEOM_QUEUE_COUNT];


namespace utils
{
    void GeomCullingGetInstVisBitBatchAndLocalID(in uint instID, out uint batchID, out uint localID)
    {
        static const uint VIS_FLAGS_BATCH_SIZE_BITS = sizeof(uint) * 8;

        batchID = instID / (VIS_FLAGS_BATCH_SIZE_BITS);
        localID = instID % (VIS_FLAGS_BATCH_SIZE_BITS);
    }
}


bool GeomCullingVisibility<let PHASE : GEOM_CULLING_PHASE>(in AABB aabb)
{
    // Runtime debug flags
    const bool isFrustumCullEnabled = DBG_IS_GEOM_GPU_FRUSTUM_CULLING_ENABLED;
    const bool isHZBCullEnabled     = DBG_IS_GEOM_GPU_HZB_CULLING_ENABLED;

    if (!isFrustumCullEnabled && !isHZBCullEnabled) {
        return true;
    }

    if (isFrustumCullEnabled) {
        if (!Frustum_Visibility(COMMON_CULLING_CAMERA_FRUSTUM, aabb)) {
            return false;
        }
    }

    if (PHASE == GEOM_CULLING_PHASE::PHASE_2) {
        if (!isHZBCullEnabled) {
            return true;
        }

        SBOX sbox = aabb.GetSBOX(HZB_CULLING_VIEW_PROJ_MATR);
        sbox.Saturate();

        if (!HZB_Visibility(sbox)) {
            return false;
        }
    }
    
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

    if (material.IsOpaque()) {
        mask |= (1u << GEOM_QUEUE_OPAQUE);
    } else if (material.IsAlphaKill()) {
        mask |= (1u << GEOM_QUEUE_AKILL);
    }

    return mask;
}


void ExecuteGeomCulling<let PHASE : GEOM_CULLING_PHASE>(in uint Gid, in uint GI, in uint DTid)
{
    if (GI < GEOM_QUEUE_COUNT) {
        GROUP_VIS_ID_QUEUE_SIZES[GI] = 0;
        GROUP_VIS_ID_QUEUE_OFFSETS[GI] = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    const uint instID = DTid;

    const bool isValidInstID = instID < COMMON_INST_COUNT;
    const bool wasVisibleInPrevFrame = isValidInstID && GeomCullingTestInstVisFlag(instID);

    uint queueLocalOffsets[GEOM_QUEUE_COUNT];

    [unroll]
    for (uint i = 0; i < GEOM_QUEUE_COUNT; ++i) {
        queueLocalOffsets[i] = ~0u;
    }

    bool shouldEnqueue = false;

    if ((PHASE == GEOM_CULLING_PHASE::PHASE_1 && wasVisibleInPrevFrame) ||
        (PHASE == GEOM_CULLING_PHASE::PHASE_2 && isValidInstID)
    ) {
        const COMMON_INST inst = COMMON_INST_BUFFER[instID];
        const bool isVisible = GeomCullingVisibility<PHASE>(inst.AABB_WCS);

        if (PHASE == GEOM_CULLING_PHASE::PHASE_1) {
            shouldEnqueue = isVisible;
        } else if (PHASE == GEOM_CULLING_PHASE::PHASE_2) {
            // Only object that is visible in the current frame
            // and was not drawn after the first pass should be added to queue in the second pass
            shouldEnqueue = isVisible && !wasVisibleInPrevFrame;

            GeomCullingWriteVisFlag(instID, isVisible);
        }

        if (shouldEnqueue) {
            uint queueMask = GeomCullingGetQueueMask(inst);

            while (queueMask != 0) {
                const uint bit = firstbitlow(queueMask);
                InterlockedAdd(GROUP_VIS_ID_QUEUE_SIZES[bit], 1u, queueLocalOffsets[bit]);

                queueMask &= (queueMask - 1u);
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (GI < GEOM_QUEUE_COUNT && GROUP_VIS_ID_QUEUE_SIZES[GI] > 0) {
        InterlockedAdd(GEOM_CULL_VIS_INST_ID_QUEUE_SIZES_UAV[GI][0], GROUP_VIS_ID_QUEUE_SIZES[GI], GROUP_VIS_ID_QUEUE_OFFSETS[GI]);
    }

    GroupMemoryBarrierWithGroupSync();

    if (shouldEnqueue) {
        for (uint queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            if (queueLocalOffsets[queue] != ~0u) {
                const uint index = GROUP_VIS_ID_QUEUE_OFFSETS[queue] + queueLocalOffsets[queue];

                GEOM_CULL_VIS_INST_ID_QUEUES_UAV[queue][index] = instID;
            }
        }
    }

    if (Gid == 0 && GI < GEOM_QUEUE_COUNT) {
        const uint groupSizeX = (uint)ceil(GEOM_CULL_VIS_INST_ID_QUEUE_SIZES_UAV[GI][0] / (float)GEOM_BATCH_CS_GROUP_SIZE);

        GEOM_CULL_BATCH_DISPATCH_CMDS_UAV[GI][0] = groupSizeX;
        GEOM_CULL_BATCH_DISPATCH_CMDS_UAV[GI][1] = groupSizeX > 0 ? 1 : 0;
        GEOM_CULL_BATCH_DISPATCH_CMDS_UAV[GI][2] = groupSizeX > 0 ? 1 : 0;
    }
}


#endif