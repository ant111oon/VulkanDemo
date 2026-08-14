#include "registers/geom_draw_cmd_gen_registers.slang"
#include "registers/common_registers.slang"


void WriteDrawCmd(RWStructuredBuffer<uint> buffer, in uint index, in GPU_CmdDrawIndexedIndirect cmd)
{
    static const uint CMD_SIZE_UINT = 5;
    
    const uint offset = index * CMD_SIZE_UINT;

    buffer[offset + 0] = cmd.indexCount;
    buffer[offset + 1] = cmd.instanceCount;
    buffer[offset + 2] = cmd.firstIndex;
    buffer[offset + 3] = asuint(cmd.firstVertex);
    buffer[offset + 4] = cmd.firstInstance;
}


[numthreads(GEOM_DRAW_CMD_GEN_CS_GROUP_SIZE, 1, 1)]
[shader("compute")]
void main(uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
    if (DTid.x >= GEOM_DRAW_CMD_GEN_BATCH_QUEUE_SIZE[0]) {
        return;
    }

    const GPU_GeomBatch batch = GEOM_DRAW_CMD_GEN_BATCH_QUEUE[DTid.x];

    const GPU_Mesh mesh = COMMON_MESH_BUFFER[batch.meshID];
    const GPU_MeshLOD lod = COMMON_MESH_LOD_BUFFER[mesh.firstLOD + batch.lodID];

    const GPU_CmdDrawIndexedIndirect cmd = GPU_CmdDrawIndexedIndirect(
        lod.indexCount,
        batch.instCount,
        lod.firstIndex,
        mesh.firstVertex,
        batch.firstInst
    );

    WriteDrawCmd(GEOM_DRAW_CMD_GEN_CMD_QUEUE_UAV, DTid.x, cmd);
}