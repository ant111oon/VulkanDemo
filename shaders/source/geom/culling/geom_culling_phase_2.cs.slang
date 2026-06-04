#define GEOM_CULLING_PHASE_2

#define GEOM_CULLING_FRUSTUM_ENABLED
#define GEOM_CULLING_HZB_ENABLED
#include "common/geom_culling_incl.slang"


groupshared uint GROUP_VIS_ID_QUEUE_SIZES[GEOM_QUEUE_COUNT];
groupshared uint GROUP_VIS_ID_QUEUE_OFFSETS[GEOM_QUEUE_COUNT];


[numthreads(GEOM_CULLING_CS_GROUP_SIZE, 1, 1)]
[shader("compute")]
void main(uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
    if (GI < GEOM_QUEUE_COUNT) {
        GROUP_VIS_ID_QUEUE_SIZES[GI] = 0;
        GROUP_VIS_ID_QUEUE_OFFSETS[GI] = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    const uint instID = DTid.x;
    const bool isValidInstID = instID < COMMON_INST_COUNT;

    uint localWriteIndices[GEOM_QUEUE_COUNT];

    [unroll]
    for (uint i = 0; i < GEOM_QUEUE_COUNT; ++i) {
        localWriteIndices[i] = ~0u;
    }

    bool shouldDraw = false;

    if (isValidInstID) {
        const COMMON_INST inst = COMMON_INST_BUFFER[instID];
        const AABB aabb = COMMON_INST_AABB_BUFFER[inst.VOLUME_IDX].GetAABB();

        const bool isVisible = GeomCullingVisibility(aabb);

        // Only object that is visible in the current frame
        // and was not drawn in the first pass should be drawn in the second pass
        shouldDraw = isVisible && !GeomCullingTestInstVisFlag(instID);
        
        GeomCullingWriteVisFlag(instID, isVisible);

        if (shouldDraw) {
            uint queueMask = GeomCullingGetQueueMask(inst);

            while (queueMask != 0) {
                const uint bit = firstbitlow(queueMask);
                InterlockedAdd(GROUP_VIS_ID_QUEUE_SIZES[bit], 1u, localWriteIndices[bit]);

                queueMask &= (queueMask - 1u);
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (GI < GEOM_QUEUE_COUNT && GROUP_VIS_ID_QUEUE_SIZES[GI] > 0) {
        InterlockedAdd(GEOM_CULL_VIS_INST_ID_QUEUE_SIZES_UAV[GI][0], GROUP_VIS_ID_QUEUE_SIZES[GI], GROUP_VIS_ID_QUEUE_OFFSETS[GI]);
    }

    GroupMemoryBarrierWithGroupSync();

    if (shouldDraw) {
        for (uint queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            if (localWriteIndices[queue] != ~0u) {
                const uint index = GROUP_VIS_ID_QUEUE_OFFSETS[queue] + localWriteIndices[queue];

                GEOM_CULL_VIS_INST_ID_QUEUES_UAV[queue][index] = instID;
            }
        }
    }
}