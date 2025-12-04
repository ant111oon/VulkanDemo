#include "registers/common_registers.slang"
#include "registers/gbuffer_culling_registers.slang"


bool IsInstVisible(in COMMON_INST_INFO instInfo)
{
    const COMMON_TRANSFORM trs = COMMON_TRANSFORMS[instInfo.TRANSFORM_IDX];
    const float3x4 wMatr = float3x4(trs.MATR[0], trs.MATR[1], trs.MATR[2]);

    const COMMON_MESH_INFO meshInfo = COMMON_MESH_INFOS[instInfo.MESH_IDX];
    AABB aabb = { meshInfo.BOUNDS_MIN_LCS, meshInfo.BOUNDS_MAX_LCS };

    const float3 newMin = mul(wMatr, float4(aabb.min, 1.f)).xyz;
    const float3 newMax = mul(wMatr, float4(aabb.max, 1.f)).xyz;

    aabb.min = min(newMin, newMax);
    aabb.max = max(newMin, newMax);

    return COMMON_CB.COMMON_CAMERA_FRUSTUM.IsIntersect(aabb); 
}


void PushCmd(in uint firstIndex, in uint indexCount, in uint firstVertex, in uint firstInstance, in uint instanceCount, in uint instID)
{
    uint idx = 0;
    InterlockedAdd(GBUFFER_CULL_INDIRECT_DRAW_CMDS_COUNT[0], 1, idx);

    GBUFFER_CULL_INDIRECT_DRAW_CMDS_UAV[idx].FIRST_INDEX = firstIndex;
    GBUFFER_CULL_INDIRECT_DRAW_CMDS_UAV[idx].INDEX_COUNT = indexCount;
    GBUFFER_CULL_INDIRECT_DRAW_CMDS_UAV[idx].VERTEX_OFFSET = firstVertex;
    GBUFFER_CULL_INDIRECT_DRAW_CMDS_UAV[idx].FIRST_INSTANCE = firstInstance;
    GBUFFER_CULL_INDIRECT_DRAW_CMDS_UAV[idx].INSTANCE_COUNT = instanceCount;

    GBUFFER_CULL_INDIRECT_DRAW_CMDS_UAV[idx].INSTANCE_INFO_IDX = instID;
}
 


[numthreads(64, 1, 1)]
[shader("compute")]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x == 0) {
        InterlockedExchange(GBUFFER_CULL_INDIRECT_DRAW_CMDS_COUNT[0], 0);
    }

    AllMemoryBarrier();

    const uint instID = DTid.x;

    if (instID >= GBUFFER_CULLING_REGISTRY.INST_COUNT) {
        return;
    }

    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[instID];
    const COMMON_MESH_INFO meshInfo = COMMON_MESH_INFOS[instInfo.MESH_IDX];

#ifdef ENV_DEBUG
    if (!COMMON_DBG_IS_MESH_GPU_CULLING_ENABLED) {
        PushCmd(meshInfo.FIRST_INDEX, meshInfo.INDEX_COUNT, meshInfo.FIRST_VERTEX, 0, 1, instID);
        return;
    }
#endif

    if (!IsInstVisible(instInfo)) {
        return;
    }

    PushCmd(meshInfo.FIRST_INDEX, meshInfo.INDEX_COUNT, meshInfo.FIRST_VERTEX, 0, 1, instID);
}