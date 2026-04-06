#include "registers/common_registers.slang"
#include "registers/mesh_culling_registers.slang"

#include "common/resource_defines.slang"


// TODO: Optimize this
AABB GetWorldAABB(in AABB aabbLCS, in float4x4 wMatr)
{
    const float3 aabbMin = aabbLCS.min;
    const float3 aabbMax = aabbLCS.max;

    float3 corners[8] = {
        float3(aabbMin.x, aabbMin.y, aabbMin.z),
        float3(aabbMax.x, aabbMin.y, aabbMin.z),
        float3(aabbMin.x, aabbMax.y, aabbMin.z),
        float3(aabbMax.x, aabbMax.y, aabbMin.z),
        float3(aabbMin.x, aabbMin.y, aabbMax.z),
        float3(aabbMax.x, aabbMin.y, aabbMax.z),
        float3(aabbMin.x, aabbMax.y, aabbMax.z),
        float3(aabbMax.x, aabbMax.y, aabbMax.z)
    };

    float3 newMin = FLOAT3(100000000.f);
    float3 newMax = FLOAT3(-100000000.f);

    for (int i = 0; i < 8; i++) {
        const float3 p = mul(wMatr, float4(corners[i], 1.f)).xyz;

        newMin = min(newMin, p);
        newMax = max(newMax, p);
    }

    AABB result;
    result.min = newMin;
    result.max = newMax;

    return result;
}


bool IsInstVisible(in COMMON_INST_INFO instInfo)
{
    const COMMON_MESH_INFO meshInfo = COMMON_MESH_INFOS[instInfo.MESH_IDX];

    const float4x4 wMatr = COMMON_TRANSFORMS[instInfo.TRANSFORM_IDX];

    AABB aabb = (AABB)0;
    aabb.min = meshInfo.BOUNDS_MIN_LCS;
    aabb.max = meshInfo.BOUNDS_MAX_LCS;

    aabb = GetWorldAABB(aabb, wMatr);

    return COMMON_CB.CAMERA_FRUSTUM.IsIntersect(aabb); 
}


void FillCommonIndirectCmd(RWStructuredBuffer<uint> dst, in uint dstIndex, in uint firstIndex, in uint indexCount, in uint firstVertex, in uint firstInstance, in uint instanceCount)
{
    // First uint in the buffer is counter
    const uint index = 1 + dstIndex * COMMON_CMD_DRAW_INDEXED_INDIRECT_SIZE_UINT;

    dst[index + 0] = indexCount;
    dst[index + 1] = instanceCount;
    dst[index + 2] = firstIndex;
    dst[index + 3] = firstVertex;
    dst[index + 4] = firstInstance;
}


void PushCmd(in COMMON_MATERIAL material, in uint firstIndex, in uint indexCount, in uint firstVertex, in uint firstInstance, in uint instanceCount, in uint instID)
{
    uint idx = 0;

    if (IS_COMMON_MATERIAL_ALPHA_BLEND(material)) {
        InterlockedAdd(MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_UAV[0], 1, idx);

        FillCommonIndirectCmd(MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_UAV, idx, firstIndex, indexCount, firstVertex, firstInstance, instanceCount);
        MESH_CULL_TRANSP_INST_INFO_IDS_UAV[idx] = instID;
    } else if (IS_COMMON_MATERIAL_ALPHA_KILL(material)) {
        InterlockedAdd(MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_UAV[0], 1, idx);

        FillCommonIndirectCmd(MESH_CULL_AKILL_INDIRECT_DRAW_CMDS_UAV, idx, firstIndex, indexCount, firstVertex, firstInstance, instanceCount);
        MESH_CULL_AKILL_INST_INFO_IDS_UAV[idx] = instID;
    } else {
        InterlockedAdd(MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_UAV[0], 1, idx);

        FillCommonIndirectCmd(MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_UAV, idx, firstIndex, indexCount, firstVertex, firstInstance, instanceCount);
        MESH_CULL_OPAQUE_INST_INFO_IDS_UAV[idx] = instID;
    }
}


[numthreads(64, 1, 1)]
[shader("compute")]
void main(uint3 DTid : SV_DispatchThreadID)
{
    const uint instID = DTid.x;

    if (instID >= PUSH_CONSTS.INST_COUNT) {
        return;
    }

    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[instID];
    const COMMON_MESH_INFO meshInfo = COMMON_MESH_INFOS[instInfo.MESH_IDX];

    const COMMON_MATERIAL material = COMMON_MATERIALS[instInfo.MATERIAL_IDX];

#ifdef ENV_DEBUG
    if (COMMON_DBG_IS_MESH_GPU_CULLING_ENABLED)
#endif
    {
        if (!IsInstVisible(instInfo)) {
            return;
        }
    }

    PushCmd(material, meshInfo.FIRST_INDEX, meshInfo.INDEX_COUNT, meshInfo.FIRST_VERTEX, 0, 1, instID);
}