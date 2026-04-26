#define GEOM_CULLING_FRUSTUM_ENABLED
#define GEOM_CULLING_CONTRIBUTION_ENABLED
#define GEOM_CULLING_HZB_ENABLED
#include "common/geom_culling_incl.slang"


void FillDrawCmd(RWStructuredBuffer<uint> dst, in uint dstIndex, in uint firstIndex, in uint indexCount, in uint firstVertex, in uint firstInstance, in uint instanceCount)
{
    static const uint CMD_SIZE_UINT = sizeof(COMMON_CMD_DRAW_INDEXED_INDIRECT) / sizeof(uint);

    // First uint in the buffer is counter
    const uint index = 1 + dstIndex * CMD_SIZE_UINT;

    dst[index + 0] = indexCount;
    dst[index + 1] = instanceCount;
    dst[index + 2] = firstIndex;
    dst[index + 3] = firstVertex;
    dst[index + 4] = firstInstance;
}


void PushInstToCmdQueue(in COMMON_INST_DATA inst, in uint instID)
{
    const COMMON_MESH_DATA mesh = COMMON_MESH_DATA_BUFFER[inst.MESH_IDX];
    const COMMON_MATERIAL material = COMMON_MATERIALS[inst.MATERIAL_IDX];

    uint idx = 0;

    if (IS_COMMON_MATERIAL_ALPHA_BLEND(material)) {
        InterlockedAdd(MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_UAV[0], 1, idx);

        FillDrawCmd(MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_UAV, idx, mesh.FIRST_INDEX, mesh.INDEX_COUNT, mesh.FIRST_VERTEX, 0, 1);
        MESH_CULL_TRANSP_INST_INFO_IDS_UAV[idx] = instID;
    } else if (IS_COMMON_MATERIAL_ALPHA_KILL(material)) {
        InterlockedAdd(MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_UAV[0], 1, idx);

        FillDrawCmd(MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_UAV, idx, mesh.FIRST_INDEX, mesh.INDEX_COUNT, mesh.FIRST_VERTEX, 0, 1);
        MESH_CULL_AKILL_INST_INFO_IDS_UAV[idx] = instID;
    } else {
        InterlockedAdd(MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_UAV[0], 1, idx);

        FillDrawCmd(MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_UAV, idx, mesh.FIRST_INDEX, mesh.INDEX_COUNT, mesh.FIRST_VERTEX, 0, 1);
        MESH_CULL_OPAQUE_INST_INFO_IDS_UAV[idx] = instID;
    }
}



[numthreads(64, 1, 1)]
[shader("compute")]
void main(uint DTid : SV_DispatchThreadID)
{
    const uint instID = DTid;

    if (instID >= MESH_CULLING_INST_COUNT) {
        return;
    }

    const COMMON_INST_DATA inst = COMMON_INST_DATA_BUFFER[instID];

    if (!Geom_Visibility(inst)) {
        return;
    }

    PushInstToCmdQueue(inst, instID);
}