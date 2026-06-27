#ifndef SM_REGISTERS_H
#define SM_REGISTERS_H

#include "common_registers.slang"
#include "common/geom_structs.slang"

#include "common/math/geom_primitives.slang"


static const uint CSM_CASCADE_COUNT = 3;
static const uint CSM_QUEUE_COUNT = CSM_CASCADE_COUNT * GEOM_QUEUE_COUNT;


struct CSM_PER_DRAW_DATA
{
    uint CASCADE_IDX;
    GEOM_QUEUE QUEUE;
};

[[vk::push_constant]] CSM_PER_DRAW_DATA CSM_DYN;

#define CSM_CASCADE_IDX     CSM_DYN.CASCADE_IDX
#define CSM_QUEUE           CSM_DYN.QUEUE
#define CSM_IS_AKILL_PASS   (CSM_QUEUE == GEOM_QUEUE::AKILL)


struct CSM_CASCADE_DATA
{
    FRUSTUM VIEW_FRUSTUM;
    row_major float4x4 VIEW_PROJ_MATRIX;
};


struct CSM_CB_DATA
{
    CSM_CASCADE_DATA CASCADES_DATA[CSM_CASCADE_COUNT];
};


[vk::binding(0, DESC_SET_PER_DRAW)] ConstantBuffer<CSM_CB_DATA> CSM_CB;

[vk::binding(1, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> CSM_VIS_INST_ID_QUEUES_UAV[CSM_QUEUE_COUNT];
[vk::binding(2, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> CSM_VIS_INST_ID_QUEUE_SIZES_UAV[CSM_QUEUE_COUNT];
[vk::binding(3, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint> CSM_BATCH_DISPATCH_CMDS_UAV[CSM_QUEUE_COUNT];

[vk::binding(4, DESC_SET_PER_DRAW)] StructuredBuffer<uint> CSM_INST_ID_QUEUE[CSM_QUEUE_COUNT];


#define CSM_CASCADE_DATA(CASCADE_IDX) CSM_CB.CASCADES_DATA[CASCADE_IDX]

#endif