#include "registers/common_registers.slang"
#include "registers/sm_registers.slang"

#include "common/geom_structs.slang"

#define FRUSTUM_CULLING_VIEW_PROJ_MATR SM_CASCADE_VIEW_PROJ_MATRIX
#include "common/math/frustum_culling.slang"

#include "common/sm_incl.slang"


uint SMGeomCullingGetQueueMask(in COMMON_INST inst)
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


groupshared uint GROUP_VIS_ID_QUEUE_SIZES[CSM_QUEUE_COUNT];
groupshared uint GROUP_VIS_ID_QUEUE_OFFSETS[CSM_QUEUE_COUNT];


[shader("compute")]
[numthreads(GEOM_CULLING_CS_GROUP_SIZE, 1, 1)]
void main(uint Gid : SV_GroupID, uint GI : SV_GroupIndex, uint DTid : SV_DispatchThreadID)
{
    if (GI < CSM_QUEUE_COUNT) {
        GROUP_VIS_ID_QUEUE_SIZES[GI] = 0;
        GROUP_VIS_ID_QUEUE_OFFSETS[GI] = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    const uint instID = DTid;
    const bool isValidInstID = instID < COMMON_INST_COUNT;

    uint queueLocalOffsets[CSM_QUEUE_COUNT];

    [unroll]
    for (uint i = 0; i < CSM_QUEUE_COUNT; ++i) {
        queueLocalOffsets[i] = ~0u;
    }

    uint isVisibleInCascadeMask = 0u;

    if (isValidInstID) {
        const COMMON_INST inst = COMMON_INST_BUFFER[instID];

        const uint queueMask = SMGeomCullingGetQueueMask(inst);

        for (uint cascade = 0; cascade < CSM_CASCADE_COUNT; ++cascade) {
            isVisibleInCascadeMask |= Frustum_Visibility(CSM_CASCADE_DATA(cascade).VIEW_FRUSTUM, inst.AABB_WCS) ? (1u << cascade) : 0u;

            if (IS_BIT_SET(isVisibleInCascadeMask, cascade)) {
                uint mask = queueMask;

                while (mask != 0) {
                    const uint bit = firstbitlow(mask);
                    const uint queue = SMGetQueueIndex(cascade, (GEOM_QUEUE)bit);

                    InterlockedAdd(GROUP_VIS_ID_QUEUE_SIZES[queue], 1u, queueLocalOffsets[queue]);

                    mask &= (mask - 1u);
                }
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (GI < CSM_QUEUE_COUNT && GROUP_VIS_ID_QUEUE_SIZES[GI] > 0) {
        InterlockedAdd(CSM_VIS_INST_ID_QUEUE_SIZES_UAV[GI][0], GROUP_VIS_ID_QUEUE_SIZES[GI], GROUP_VIS_ID_QUEUE_OFFSETS[GI]);
    }

    GroupMemoryBarrierWithGroupSync();

    for (uint cascade = 0; cascade < CSM_CASCADE_COUNT; ++cascade) {
        if (IS_BIT_SET(isVisibleInCascadeMask, cascade)) {
            for (uint queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
                const uint queueIndex = SMGetQueueIndex(cascade, (GEOM_QUEUE)queue);

                if (queueLocalOffsets[queueIndex] != ~0u) {
                    const uint index = GROUP_VIS_ID_QUEUE_OFFSETS[queueIndex] + queueLocalOffsets[queueIndex];

                    CSM_VIS_INST_ID_QUEUES_UAV[queueIndex][index] = instID;
                }
            }
        }
    }

    if (Gid == 0 && GI < CSM_QUEUE_COUNT) {
        const uint groupSizeX = (uint)ceil(CSM_VIS_INST_ID_QUEUE_SIZES_UAV[GI][0] / (float)GEOM_BATCH_CS_GROUP_SIZE);

        CSM_BATCH_DISPATCH_CMDS_UAV[GI][0] = groupSizeX;
        CSM_BATCH_DISPATCH_CMDS_UAV[GI][1] = groupSizeX > 0 ? 1 : 0;
        CSM_BATCH_DISPATCH_CMDS_UAV[GI][2] = groupSizeX > 0 ? 1 : 0;
    }
}