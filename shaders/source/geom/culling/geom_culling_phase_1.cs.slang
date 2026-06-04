#define GEOM_CULLING_PHASE_1

#define GEOM_CULLING_FRUSTUM_ENABLED
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

    const bool wasVisibleInPrevFrame = instID < COMMON_INST_COUNT && GeomCullingTestInstVisFlag(instID);

    uint localWriteIndices[GEOM_QUEUE_COUNT];

    [unroll]
    for (uint i = 0; i < GEOM_QUEUE_COUNT; ++i) {
        localWriteIndices[i] = ~0u;
    }

    bool isVisible = false;

    if (wasVisibleInPrevFrame) {
        const COMMON_INST inst = COMMON_INST_BUFFER[instID];
        const AABB aabb = COMMON_INST_AABB_BUFFER[inst.VOLUME_IDX].GetAABB();

        isVisible = GeomCullingVisibility(aabb);

        if (isVisible) {
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

    if (isVisible) {
        for (uint queue = 0; queue < GEOM_QUEUE_COUNT; ++queue) {
            if (localWriteIndices[queue] != ~0u) {
                const uint index = GROUP_VIS_ID_QUEUE_OFFSETS[queue] + localWriteIndices[queue];

                GEOM_CULL_VIS_INST_ID_QUEUES_UAV[queue][index] = instID;
            }
        }
    }
}