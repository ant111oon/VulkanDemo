#ifndef CSM_REGISTERS_H
#define CSM_REGISTERS_H

#include "common_registers.slang"
#include "common/common_geom.slang"

#include "common/math/geom_primitives.slang"


static const uint CSM_BUFFER_COUNT = COMMON_CSM_CASCADE_COUNT * GEOM_QUEUE_COUNT;


struct GPU_CsmPerDrawData
{
    uint cascadeIdx;
    GPU_GeomQueue queue;
};

[[vk::push_constant]] GPU_CsmPerDrawData CSM_DYN;

#define CSM_CASCADE_IDX     CSM_DYN.cascadeIdx
#define CSM_QUEUE           CSM_DYN.queue
#define CSM_IS_AKILL_PASS   (CSM_QUEUE == GPU_GeomQueue::AKILL)


[vk::binding(0, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> CSM_VIS_INST_ID_QUEUES_UAV[CSM_BUFFER_COUNT];
[vk::binding(1, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> CSM_VIS_INST_ID_QUEUE_SIZES_UAV[CSM_BUFFER_COUNT];

[vk::binding(2, DESC_SET_PER_DRAW)] StructuredBuffer<uint> CSM_INST_ID_QUEUE[CSM_BUFFER_COUNT];

#endif