#include "registers/geom_draw_cmd_gen_registers.slang"
#include "registers/common_registers.slang"


void WriteDrawCmd(RWStructuredBuffer<uint> buffer, in uint index, in COMMON_CMD_DRAW_INDEXED_INDIRECT cmd)
{
    static const uint CMD_SIZE_UINT = 5;
    
    const uint offset = index * CMD_SIZE_UINT;

    buffer[offset + 0] = cmd.INDEX_COUNT;
    buffer[offset + 1] = cmd.INSTANCE_COUNT;
    buffer[offset + 2] = cmd.FIRST_INDEX;
    buffer[offset + 3] = asuint(cmd.FIRST_VERTEX);
    buffer[offset + 4] = cmd.FIRST_INSTANCE;
}


[numthreads(GEOM_DRAW_CMD_GEN_CS_GROUP_SIZE, 1, 1)]
[shader("compute")]
void main(uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
    if (DTid.x >= GEOM_DRAW_CMD_GEN_BATCH_QUEUE_SIZE[0]) {
        return;
    }

    const GEOM_BATCH batch = GEOM_DRAW_CMD_GEN_BATCH_QUEUE[DTid.x];

    const COMMON_MESH mesh = COMMON_MESH_BUFFER[batch.MESH_ID];
    const COMMON_MESH_LOD lod = COMMON_MESH_LOD_BUFFER[mesh.FIRST_LOD + batch.LOD_ID];

    const COMMON_CMD_DRAW_INDEXED_INDIRECT cmd = COMMON_CMD_DRAW_INDEXED_INDIRECT(
        lod.INDEX_COUNT, 
        batch.INST_COUNT, 
        lod.FIRST_INDEX, 
        mesh.FIRST_VERTEX, 
        batch.FIRST_INST
    );

    WriteDrawCmd(GEOM_DRAW_CMD_GEN_CMD_QUEUE_UAV, DTid.x, cmd);
}