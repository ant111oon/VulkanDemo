#include "registers/geom_batching_registers.slang"
#include "registers/common_registers.slang"

#include "common/common_geom_structs.slang"


[shader("compute")]
[numthreads(GEOM_BATCH_CS_GROUP_SIZE, 1, 1)]
void main(SV_DISPATCH_THREAD_ID(dispatchThreadID))
{
    const uint batchID = dispatchThreadID.x;

    const uint batchCount = GEOM_BATCH_COUNT[0];

    if (batchID >= batchCount) {
        return;
    }

    const uint firstInst = GEOM_BATCH_FIRST_INST[batchID];

    const uint nextFirstInst = batchID + 1 < batchCount ? GEOM_BATCH_FIRST_INST[batchID + 1] : GEOM_BATCH_SORTED_VIS_INST_COUNT[0];
    const uint instCount = nextFirstInst - firstInst;

    const GPU_GeomSortKey key = GEOM_BATCH_SORTED_KEYS[firstInst];

    const GPU_Mesh mesh = COMMON_MESH_BUFFER[key.meshID];
    const GPU_MeshLOD lod = COMMON_MESH_LOD_BUFFER[mesh.firstLOD + key.lodID];

    const uint matType = (uint)key.matType;

    const uint firstBatchOfMatType = GEOM_MAT_TYPE_FIRST_BATCH[matType];
    const uint localBatchID = batchID - firstBatchOfMatType;

    const uint dstCmdIndex = GEOM_BATCH_CONST.cmdWriteOffsets[matType] + localBatchID;

    GPU_CmdDrawIndexedIndirect cmd;

    cmd.indexCount    = lod.indexCount;
    cmd.instanceCount = instCount;

    cmd.firstIndex    = lod.firstIndex;
    cmd.vertexOffset  = int(mesh.firstVertex);

    cmd.firstInstance = firstInst;

    GEOM_BATCH_DRAW_CMDS_UAV[dstCmdIndex] = cmd;

    bool lastBatchOfMatType = batchID == batchCount - 1;

    if (!lastBatchOfMatType) {
        const uint nextFirst = GEOM_BATCH_FIRST_INST[batchID + 1];
        const uint nextMatType = (uint)GEOM_BATCH_SORTED_KEYS[nextFirst].matType;

        lastBatchOfMatType = nextMatType != matType;
    }

    if (lastBatchOfMatType) {
        GEOM_BATCH_DRAW_CMD_COUNTS_UAV[matType] = localBatchID + 1;
    }
}