#ifndef GEOM_SORT_REGISTERS_H
#define GEOM_SORT_REGISTERS_H

#include "common/system/sys_constants.slang"
#include "common/common_geom.slang"


struct GPU_GeomSortPerDrawData
{
    uint bitOffset;
    uint groupCount;
    uint srcBufferIndex;
};

[[vk::push_constant]] GPU_GeomSortPerDrawData GEOM_SORT_DYN;

#define GEOM_SORT_BIT_OFFSET        GEOM_SORT_DYN.bitOffset
#define GEOM_SORT_GROUP_COUNT       GEOM_SORT_DYN.groupCount
#define GEOM_SORT_SRC_BUFFER_IDX    clamp(GEOM_SORT_DYN.srcBufferIndex, 0u, 1u)
#define GEOM_SORT_DST_BUFFER_IDX    (1u - GEOM_SORT_SRC_BUFFER_IDX)

#define GEOM_SORT_SRC_KEY_BUFFER        GEOM_SORT_KEY_BUFFERS_UAV[GEOM_SORT_SRC_BUFFER_IDX]
#define GEOM_SORT_DST_KEY_BUFFER        GEOM_SORT_KEY_BUFFERS_UAV[GEOM_SORT_DST_BUFFER_IDX]
#define GEOM_SORT_SRC_INST_ID_BUFFER    GEOM_SORT_INST_ID_BUFFERS_UAV[GEOM_SORT_SRC_BUFFER_IDX]
#define GEOM_SORT_DST_INST_ID_BUFFER    GEOM_SORT_INST_ID_BUFFERS_UAV[GEOM_SORT_DST_BUFFER_IDX]


[vk::binding(0, DESC_SET_PER_DRAW)] RWStructuredBuffer<GPU_GeomSortKey> GEOM_SORT_KEY_BUFFERS_UAV[2];
[vk::binding(1, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint>            GEOM_SORT_INST_ID_BUFFERS_UAV[2];
[vk::binding(2, DESC_SET_PER_DRAW)] StructuredBuffer<uint>              GEOM_SORT_VIS_INST_COUNT;

[vk::binding(3, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint>            GEOM_SORT_GROUP_OFFSETS_UAV;
[vk::binding(4, DESC_SET_PER_DRAW)] RWStructuredBuffer<uint>            GEOM_SORT_BUCKET_BASES_UAV;

#endif