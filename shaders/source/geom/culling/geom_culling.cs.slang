#include "registers/common_registers.slang"
#include "registers/geom_culling_registers.slang"

#include "common/math/frustum_culling.slang"

#define HZB_CULLING_HZB_TEX_SAMPLER GET_COMMON_SAMPLER(GPU_CommonSamplerID::NEAREST_CLAMP_TO_EDGE)
#define HZB_CULLING_HZB_TEX         COMMON_HZB
#define HZB_CULLING_HZB_MIP_0_SIZE  COMMON_SCREEN_SIZE
#define HZB_CULLING_HZB_MIPS_COUNT  GEOM_CULLING_HZB_MIPS_COUNT
#define HZB_CULLING_VIEW_PROJ_MATR  COMMON_CULLING_VIEW_PROJ_MATRIX_PREV
#define HZB_CULLING_VIEW_NEAR_PLANE COMMON_CULLING_FRUSTUM.GetPlane(GPU_Frustum::PlaneID::PLANE_NEAR)
#include "common/math/hzb_culling.slang"


bool GeomCullingVisibility(in GPU_AABB aabb)
{
    if (COMMON_DBG_IS_GEOM_GPU_FRUSTUM_CULLING_ENABLED) {
        if (!Frustum_Visibility(COMMON_CULLING_FRUSTUM, aabb)) {
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


uint GeomCullingGetQueueMask(in GPU_GeomInst inst)
{
    uint mask = 0;

    const GPU_GeomMaterial material = COMMON_MATERIALS[inst.materialID];

    if (material.isOpaque) {
        mask |= (1u << GEOM_QUEUE_OPAQUE);
    } else if (material.isAlphaKill) {
        mask |= (1u << GEOM_QUEUE_AKILL);
    }

    return mask;
}


GPU_GeomSortKey GeomGetSortKey(in GPU_GeomInst inst)
{
    const float depth = -mul(COMMON_MAIN_CAM_VIEW_MATRIX, float4(inst.aabbWCS.GetCenter(), 1.f)).z;

    uint lodID;

    if (COMMON_DBG_FORCED_GEOM_LOD >= 0) {
        lodID = COMMON_DBG_FORCED_GEOM_LOD;
    } else {
        // Calculate fair LOD
        lodID = 0;
    }

    lodID = clamp(lodID, 0, COMMON_MESH_BUFFER[inst.meshID].lodCount - 1);

    const GPU_GeomMaterial material = COMMON_MATERIALS[inst.materialID];

    return GPU_GeomSortKey(material.type, inst.meshID, lodID, depth);
}


groupshared uint GROUP_CULL_VIS_INST_COUNTER;
groupshared uint GROUP_CULL_VIS_INST_OFFSET;
groupshared uint GROUP_CULL_PER_MAT_TYPE_COUNTERS[GPU_GeomMatType::COUNT];

groupshared uint GROUP_VIS_ID_QUEUE_SIZES[GEOM_QUEUE_COUNT];
groupshared uint GROUP_VIS_ID_QUEUE_OFFSETS[GEOM_QUEUE_COUNT];

// #define GEOM_NEW_INSTANCING

[numthreads(GEOM_CULLING_CS_GROUP_SIZE, 1, 1)]
[shader("compute")]
void main(in uint Gid : SV_GroupID, in uint GI : SV_GroupIndex, in uint DTid : SV_DispatchThreadID)
{
#ifndef GEOM_NEW_INSTANCING
    if (GI < GPU_GeomQueue::COUNT) {
        GROUP_VIS_ID_QUEUE_SIZES[GI] = 0;
        GROUP_VIS_ID_QUEUE_OFFSETS[GI] = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    const uint instID = DTid;

    const bool isValidInstID = instID < COMMON_INST_COUNT;

    uint queueLocalOffsets[GPU_GeomQueue::COUNT];

    GPU_GeomInst inst;
    bool isVisible = false;

    if (isValidInstID) {
        inst = COMMON_INST_BUFFER[instID];
        isVisible = GeomCullingVisibility(inst.aabbWCS);
    }

    if (isVisible) {
        [unroll]
        for (uint i = 0; i < GPU_GeomQueue::COUNT; ++i) {
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

    if (GI < GPU_GeomQueue::COUNT && GROUP_VIS_ID_QUEUE_SIZES[GI] > 0) {
        InterlockedAdd(GEOM_CULL_VIS_INST_ID_QUEUE_SIZES_UAV[GI][0], GROUP_VIS_ID_QUEUE_SIZES[GI], GROUP_VIS_ID_QUEUE_OFFSETS[GI]);
    }

    GroupMemoryBarrierWithGroupSync();

    if (isVisible) {
        for (uint queue = 0; queue < GPU_GeomQueue::COUNT; ++queue) {
            if (queueLocalOffsets[queue] != ~0u) {
                const uint index = GROUP_VIS_ID_QUEUE_OFFSETS[queue] + queueLocalOffsets[queue];

                GEOM_CULL_VIS_INST_ID_QUEUES_UAV[queue][index] = instID;
            }
        }
    }
#else
    if (GI == 0) {
        GROUP_CULL_VIS_INST_COUNTER = 0;
        GROUP_CULL_VIS_INST_OFFSET = 0;
    }

    if (GI < GPU_GeomMatType::COUNT) {
        GROUP_CULL_PER_MAT_TYPE_COUNTERS[GI] = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    const uint instID = DTid;

    GPU_GeomSortKey sortKey;
    GPU_GeomInst inst;

    bool isVisible = false;

    if (instID < COMMON_INST_COUNT) {
        inst = COMMON_INST_BUFFER[instID];
        isVisible = GeomCullingVisibility(inst.aabbWCS);
    }

    uint groupLocalIndex = 0;

    if (isVisible) {
        sortKey = GeomGetSortKey(inst);

        InterlockedAdd(GROUP_CULL_VIS_INST_COUNTER, 1, groupLocalIndex);
        InterlockedAdd(GROUP_CULL_PER_MAT_TYPE_COUNTERS[sortKey.matType], 1);
    }

    GroupMemoryBarrierWithGroupSync();

    if (GI == 0 && GROUP_CULL_VIS_INST_COUNTER > 0) { ;
        InterlockedAdd(GEOM_CULL_VIS_SORT_KEYS_COUNTER_UAV[0], GROUP_CULL_VIS_INST_COUNTER, GROUP_CULL_VIS_INST_OFFSET);
    }

    if (GI < GPU_GeomMatType::COUNT) {
        const uint count = GROUP_CULL_PER_MAT_TYPE_COUNTERS[GI];

        if (count > 0) {
            InterlockedAdd(GEOM_CULL_PER_GEOM_MAT_TYPE_COUNTERS_UAV[GI], count);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (isVisible) {
        const uint index = GROUP_CULL_VIS_INST_OFFSET + groupLocalIndex;

        GEOM_CULL_VIS_SORT_KEYS_UAV[index] = sortKey;
        GEOM_CULL_VIS_GEOM_IDS_UAV[index] = instID;
    }
#endif
}