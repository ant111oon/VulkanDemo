#include "registers/common_registers.slang"
#include "registers/csm_registers.slang"

#include "common/geom_structs.slang"

#define FRUSTUM_CULLING_VIEW_PROJ_MATR SM_CASCADE_VIEW_PROJ_MATRIX
#include "common/math/frustum_culling.slang"

#include "common/csm_incl.slang"


groupshared uint GROUP_VIS_ID_QUEUE_SIZES[CSM_BUFFER_COUNT];
groupshared uint GROUP_VIS_ID_QUEUE_OFFSETS[CSM_BUFFER_COUNT];


[shader("compute")]
[numthreads(GEOM_CULLING_CS_GROUP_SIZE, 1, 1)]
void main(uint Gid : SV_GroupID, uint GI : SV_GroupIndex, uint DTid : SV_DispatchThreadID)
{
    if (GI < CSM_BUFFER_COUNT) {
        GROUP_VIS_ID_QUEUE_SIZES[GI] = 0;
        GROUP_VIS_ID_QUEUE_OFFSETS[GI] = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    const uint instID = DTid;
    const bool isValidInstID = instID < COMMON_INST_COUNT;

    uint queueLocalOffsets[CSM_BUFFER_COUNT];

    [unroll]
    for (uint i = 0; i < CSM_BUFFER_COUNT; ++i) {
        queueLocalOffsets[i] = ~0u;
    }

    uint isVisibleInCascadeMask = 0u;

    if (isValidInstID) {
        const COMMON_INST inst = COMMON_INST_BUFFER[instID];

        const uint queueMask = CSMGeomCullingGetQueueMask(inst);

        for (uint cascade = 0; cascade < COMMON_CSM_CASCADE_COUNT;) {
            isVisibleInCascadeMask |= Frustum_Visibility(COMMON_CSM_VIEW_FRUSTUM(cascade), inst.AABB_WCS) ? (1u << cascade) : 0u;

            if (IS_BIT_SET(isVisibleInCascadeMask, cascade)) {
                uint mask = queueMask;

                while (mask != 0) {
                    const uint bit = firstbitlow(mask);
                    const uint queue = CSMGetBufferIndex(cascade, (GEOM_QUEUE)bit);

                    InterlockedAdd(GROUP_VIS_ID_QUEUE_SIZES[queue], 1u, queueLocalOffsets[queue]);

                    mask &= (mask - 1u);
                }
            }

            ++cascade;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (GI < CSM_BUFFER_COUNT && GROUP_VIS_ID_QUEUE_SIZES[GI] > 0) {
        InterlockedAdd(CSM_VIS_INST_ID_QUEUE_SIZES_UAV[GI][0], GROUP_VIS_ID_QUEUE_SIZES[GI], GROUP_VIS_ID_QUEUE_OFFSETS[GI]);
    }

    GroupMemoryBarrierWithGroupSync();

    for (uint cascade = 0; cascade < COMMON_CSM_CASCADE_COUNT; ++cascade) {
        if (IS_BIT_SET(isVisibleInCascadeMask, cascade)) {
            for (uint queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
                const uint queueIndex = CSMGetBufferIndex(cascade, (GEOM_QUEUE)queue);

                if (queueLocalOffsets[queueIndex] != ~0u) {
                    const uint index = GROUP_VIS_ID_QUEUE_OFFSETS[queueIndex] + queueLocalOffsets[queueIndex];

                    CSM_VIS_INST_ID_QUEUES_UAV[queueIndex][index] = instID;
                }
            }
        }
    }

    if (Gid == 0 && GI < CSM_BUFFER_COUNT) {
        const uint groupSizeX = (uint)ceil(CSM_VIS_INST_ID_QUEUE_SIZES_UAV[GI][0] / (float)GEOM_BATCH_CS_GROUP_SIZE);

        CSM_BATCH_DISPATCH_CMDS_UAV[GI][0] = groupSizeX;
        CSM_BATCH_DISPATCH_CMDS_UAV[GI][1] = groupSizeX > 0 ? 1 : 0;
        CSM_BATCH_DISPATCH_CMDS_UAV[GI][2] = groupSizeX > 0 ? 1 : 0;
    }
}