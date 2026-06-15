#include "registers/geom_batching_registers.slang"
#include "registers/common_registers.slang"


groupshared uint GROUP_BATCH_SIZE[GEOM_BATCH_CS_GROUP_SIZE];
groupshared uint GROUP_BATCH_FIRST_INST_LOCAL[GEOM_BATCH_CS_GROUP_SIZE];

groupshared uint GROUP_BATCH_FIRST_INST_ACCUM;
groupshared uint GROUP_BATCH_FIRST_INST_GLOBAL;


[numthreads(GEOM_BATCH_CS_GROUP_SIZE, 1, 1)]
[shader("compute")]
void main(uint Gid : SV_GroupID, uint GI : SV_GroupIndex, uint DTid : SV_DispatchThreadID)
{
    if (GI == 0) {
        GROUP_BATCH_FIRST_INST_ACCUM = 0;
    }

    if (GI < GEOM_BATCH_CS_GROUP_SIZE) {
        GROUP_BATCH_SIZE[GI] = 0;
        GROUP_BATCH_FIRST_INST_LOCAL[GI] = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    const uint instCount = GEOM_BATCH_VIS_INST_ID_QUEUE_SIZE[0];

    uint instID = ~0u;
    uint localIndex = ~0u;
    uint meshID = ~0u;

    if (DTid < instCount) {
        instID = GEOM_BATCH_VIS_INST_ID_QUEUE[DTid];

        meshID = COMMON_INST_BUFFER[instID].MESH_IDX;

        if (meshID < GEOM_BATCH_CS_GROUP_SIZE) {
            InterlockedAdd(GROUP_BATCH_SIZE[meshID], 1, localIndex);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (GROUP_BATCH_SIZE[GI] > 0) {
        InterlockedAdd(GROUP_BATCH_FIRST_INST_ACCUM, GROUP_BATCH_SIZE[GI], GROUP_BATCH_FIRST_INST_LOCAL[GI]);
    }

    GroupMemoryBarrierWithGroupSync();

    if (GI == 0) {
        InterlockedAdd(GEOM_BATCH_SORTED_VIS_INST_ID_QUEUE_SIZE_UAV[0], GROUP_BATCH_FIRST_INST_ACCUM, GROUP_BATCH_FIRST_INST_GLOBAL);
    }

    GroupMemoryBarrierWithGroupSync();

    if (DTid < instCount && localIndex != ~0u) {
        const uint writeIdx = GROUP_BATCH_FIRST_INST_GLOBAL + GROUP_BATCH_FIRST_INST_LOCAL[meshID] + localIndex;
        GEOM_BATCH_SORTED_VIS_INST_ID_QUEUE_UAV[writeIdx] = instID;
    }

    if (GROUP_BATCH_SIZE[GI] > 0) {
        uint writeIdx;
        InterlockedAdd(GEOM_BATCH_BATCH_QUEUE_SIZE_UAV[0], 1, writeIdx);

        const uint lodID = clamp(COMMON_DBG_FORCED_GEOM_LOD >= 0 ? COMMON_DBG_FORCED_GEOM_LOD : 0, 0, COMMON_MESH_BUFFER[GI].LOD_COUNT - 1);

        const GEOM_BATCH batch = GEOM_BATCH(
            GI,
            lodID,
            GROUP_BATCH_FIRST_INST_GLOBAL + GROUP_BATCH_FIRST_INST_LOCAL[GI], 
            GROUP_BATCH_SIZE[GI]
        );

        GEOM_BATCH_BATCH_QUEUE_UAV[writeIdx] = batch;
    }

    GroupMemoryBarrierWithGroupSync();

    if (DTid == 0) {
        const uint groupSizeX = (uint)ceil(GEOM_BATCH_BATCH_QUEUE_SIZE_UAV[0] / (float)GEOM_DRAW_CMD_GEN_CS_GROUP_SIZE);

        GEOM_BATCH_DRAW_CMD_GEN_DISPATCH_CMD_UAV[0] = groupSizeX;
        GEOM_BATCH_DRAW_CMD_GEN_DISPATCH_CMD_UAV[1] = groupSizeX > 0 ? 1 : 0;
        GEOM_BATCH_DRAW_CMD_GEN_DISPATCH_CMD_UAV[2] = groupSizeX > 0 ? 1 : 0;
    }
}