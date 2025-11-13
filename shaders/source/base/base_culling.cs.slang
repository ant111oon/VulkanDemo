#include "common_registers.slang"


struct AABB
{
    float3 GetCenter() { return (minimum + maximum) * 0.5; }
    float3 GetExtents() { return (maximum - minimum) * 0.5; }

    float3 minimum;
    float3 maximum;
};


bool IsInstVisible(in COMMON_INST_INFO instInfo)
{
    const COMMON_TRANSFORM trs = COMMON_TRANSFORMS[instInfo.TRANSFORM_IDX];
    const float3x4 wMatr = float3x4(trs.MATR[0], trs.MATR[1], trs.MATR[2]);

    const COMMON_MESH_INFO meshInfo = COMMON_MESH_INFOS[instInfo.MESH_IDX];
    AABB aabb = { meshInfo.BOUNDS_MIN_LCS, meshInfo.BOUNDS_MAX_LCS };

    const float3 newMin = mul(wMatr, float4(aabb.minimum, 1.f)).xyz;
    const float3 newMax = mul(wMatr, float4(aabb.maximum, 1.f)).xyz;

    aabb.minimum = min(newMin, newMax);
    aabb.maximum = max(newMin, newMax);

    // TODO: Replace with correct Frustum-AABB intersection algorithm
    float3 corners[8] = {
        float3(aabb.minimum.x, aabb.minimum.y, aabb.minimum.z),
        float3(aabb.maximum.x, aabb.minimum.y, aabb.minimum.z),
        float3(aabb.minimum.x, aabb.maximum.y, aabb.minimum.z),
        float3(aabb.maximum.x, aabb.maximum.y, aabb.minimum.z),
        float3(aabb.minimum.x, aabb.minimum.y, aabb.maximum.z),
        float3(aabb.maximum.x, aabb.minimum.y, aabb.maximum.z),
        float3(aabb.minimum.x, aabb.maximum.y, aabb.maximum.z),
        float3(aabb.maximum.x, aabb.maximum.y, aabb.maximum.z)
    };

    for (uint i = 0; i < 8; ++i) {
        const float4 clipCoords = mul(COMMON_CB.COMMON_VIEW_PROJ_MATRIX, float4(corners[i], 1.f));
        const float3 ndc = clipCoords.xyz / clipCoords.w;

        if (abs(ndc.x) <= 1.f && abs(ndc.y) <= 1.f && ndc.z >= 0.f && ndc.z <= 1.f) {
            return true;
        }
    }

    return false;
}


void PushCmd(in uint firstIndex, in uint indexCount, in uint firstVertex, in uint firstInstance, in uint instanceCount, in uint instID)
{
    uint idx = 0;
    InterlockedAdd(BASE_INDIRECT_DRAW_CMDS_COUNT[0], 1, idx);

    BASE_INDIRECT_DRAW_CMDS_UAV[idx].FIRST_INDEX = firstIndex;
    BASE_INDIRECT_DRAW_CMDS_UAV[idx].INDEX_COUNT = indexCount;
    BASE_INDIRECT_DRAW_CMDS_UAV[idx].VERTEX_OFFSET = firstVertex;
    BASE_INDIRECT_DRAW_CMDS_UAV[idx].FIRST_INSTANCE = firstInstance;
    BASE_INDIRECT_DRAW_CMDS_UAV[idx].INSTANCE_COUNT = instanceCount;

    BASE_INDIRECT_DRAW_CMDS_UAV[idx].INSTANCE_INFO_IDX = instID;
}


struct BASE_CULLING_BINDLESS_REGISTRY
{
    float3 PAD0;
    uint   INST_COUNT;
};

[[vk::push_constant]] BASE_CULLING_BINDLESS_REGISTRY CULLING_REGISTRY; 


[numthreads(64, 1, 1)]
[shader("compute")]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x == 0) {
        InterlockedExchange(BASE_INDIRECT_DRAW_CMDS_COUNT[0], 0);
    }

    AllMemoryBarrier();

    const uint instID = DTid.x;

    if (instID >= CULLING_REGISTRY.INST_COUNT) {
        return;
    }

    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[instID];
    const COMMON_MESH_INFO meshInfo = COMMON_MESH_INFOS[instInfo.MESH_IDX];

#ifdef ENV_DEBUG
    if (!COMMON_DBG_IS_MESH_GPU_CULLING) {
        PushCmd(meshInfo.FIRST_INDEX, meshInfo.INDEX_COUNT, meshInfo.FIRST_VERTEX, 0, 1, instID);
        return;
    }
#endif

    if (!IsInstVisible(instInfo)) {
        return;
    }

    PushCmd(meshInfo.FIRST_INDEX, meshInfo.INDEX_COUNT, meshInfo.FIRST_VERTEX, 0, 1, instID);
}