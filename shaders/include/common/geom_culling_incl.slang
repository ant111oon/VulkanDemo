#ifndef GEOM_CULLING_INCL_H
#define GEOM_CULLING_INCL_H


#include "registers/common_registers.slang"
#include "registers/geom_culling_registers.slang"

#include "common/math/frustum_culling.slang"

#define HZB_CULLING_HZB_TEX_SAMPLER GET_COMMON_SAMPLER(COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE)
#define HZB_CULLING_HZB_TEX         COMMON_HZB
#define HZB_CULLING_HZB_MIP_0_SIZE  COMMON_SCREEN_SIZE
#define HZB_CULLING_HZB_MIPS_COUNT  GEOM_CULLING_HZB_MIPS_COUNT
#define HZB_CULLING_VIEW_PROJ_MATR  COMMON_CULLING_VIEW_PROJ_MATRIX_PREV
#define HZB_CULLING_VIEW_NEAR_PLANE COMMON_CULLING_CAMERA_FRUSTUM.GetPlane(FRUSTUM::PLANE_ID::PLANE_NEAR)
#include "common/math/hzb_culling.slang"


groupshared uint GROUP_VIS_ID_QUEUE_SIZES[GEOM_QUEUE_COUNT];
groupshared uint GROUP_VIS_ID_QUEUE_OFFSETS[GEOM_QUEUE_COUNT];


bool GeomCullingVisibility(in AABB aabb)
{
    if (COMMON_DBG_IS_GEOM_GPU_FRUSTUM_CULLING_ENABLED) {
        if (!Frustum_Visibility(COMMON_CULLING_CAMERA_FRUSTUM, aabb)) {
            return false;
        }
    }

    if (COMMON_DBG_IS_GEOM_GPU_HZB_CULLING_ENABLED) {
        if (!HZB_Visibility(aabb)) {
            return false;
        }
    }
    
    return true; 
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

void ExecuteGeomCulling(in uint Gid, in uint GI, in uint DTid)
{
    if (GI < GEOM_QUEUE::COUNT) {
        GROUP_VIS_ID_QUEUE_SIZES[GI] = 0;
        GROUP_VIS_ID_QUEUE_OFFSETS[GI] = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    const uint instID = DTid;

    const bool isValidInstID = instID < COMMON_INST_COUNT;

    uint queueLocalOffsets[GEOM_QUEUE::COUNT];

    COMMON_INST inst;
    bool isVisible = false;

    if (isValidInstID) {
        inst = COMMON_INST_BUFFER[instID];
        isVisible = GeomCullingVisibility(inst.AABB_WCS);
    }

    if (isVisible) {
        [unroll]
        for (uint i = 0; i < GEOM_QUEUE::COUNT; ++i) {
            queueLocalOffsets[i] = ~0u;
        }

        uint queueMask = GeomCullingGetQueueMask(inst);

        while (queueMask != 0) {
            const uint bit = firstbitlow(queueMask);
            InterlockedAdd(GROUP_VIS_ID_QUEUE_SIZES[bit], 1u, queueLocalOffsets[bit]);

            queueMask &= (queueMask - 1u);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (GI < GEOM_QUEUE::COUNT && GROUP_VIS_ID_QUEUE_SIZES[GI] > 0) {
        InterlockedAdd(GEOM_CULL_VIS_INST_ID_QUEUE_SIZES_UAV[GI][0], GROUP_VIS_ID_QUEUE_SIZES[GI], GROUP_VIS_ID_QUEUE_OFFSETS[GI]);
    }

    GroupMemoryBarrierWithGroupSync();

    if (isVisible) {
        for (uint queue = 0; queue < GEOM_QUEUE::COUNT; ++queue) {
            if (queueLocalOffsets[queue] != ~0u) {
                const uint index = GROUP_VIS_ID_QUEUE_OFFSETS[queue] + queueLocalOffsets[queue];

                GEOM_CULL_VIS_INST_ID_QUEUES_UAV[queue][index] = instID;
            }
        }
    }
}


#endif