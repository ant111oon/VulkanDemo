#include "registers/common_registers.slang"
#include "registers/mesh_culling_registers.slang"

#include "common/common_defines.slang"


bool IsInstVisible(in COMMON_INST_INFO instInfo)
{
    const COMMON_MESH_INFO meshInfo = COMMON_MESH_INFOS[instInfo.MESH_IDX];

    const float4x4 wMatr = COMMON_TRANSFORMS[instInfo.TRANSFORM_IDX];

    SPHERE sphere = (SPHERE)0;
    sphere.position = mul(wMatr, float4(meshInfo.SPHERE_BOUNDS_CENTER_LCS, 1.f)).xyz;

    const float scale = max(max(length(wMatr[0].xyz), length(wMatr[1].xyz)), length(wMatr[2].xyz));
    sphere.radius = scale * meshInfo.SPHERE_BOUNDS_RADIUS_LCS;

    return COMMON_CB.COMMON_CAMERA_FRUSTUM.IsIntersect(sphere); 
}


COMMON_INDIRECT_DRAW_CMD CreateCommonIndirectCmd(in uint firstIndex, in uint indexCount, in uint firstVertex, in uint firstInstance, in uint instanceCount)
{
    COMMON_INDIRECT_DRAW_CMD cmd = (COMMON_INDIRECT_DRAW_CMD)0;
    cmd.FIRST_INDEX = firstIndex;
    cmd.INDEX_COUNT = indexCount;
    cmd.VERTEX_OFFSET = firstVertex;
    cmd.FIRST_INSTANCE = firstInstance;
    cmd.INSTANCE_COUNT = instanceCount;

    return cmd;
}


void PushCmd(in uint firstIndex, in uint indexCount, in uint firstVertex, in uint firstInstance, in uint instanceCount, in uint instID, in bool isTransp)
{
    if (isTransp) {
        uint idx = 0;
        InterlockedAdd(MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_COUNT_UAV[0], 1, idx);

        MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_UAV[idx] = CreateCommonIndirectCmd(firstIndex, indexCount, firstVertex, firstInstance, instanceCount);
        MESH_CULL_TRANSP_INST_INFO_IDS_UAV[idx] = instID;
    } else {
        uint idx = 0;
        InterlockedAdd(MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_COUNT_UAV[0], 1, idx);

        MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_UAV[idx] = CreateCommonIndirectCmd(firstIndex, indexCount, firstVertex, firstInstance, instanceCount);
        MESH_CULL_OPAQUE_INST_INFO_IDS_UAV[idx] = instID;
    }
}


[numthreads(64, 1, 1)]
[shader("compute")]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x == 0) {
        InterlockedExchange(MESH_CULL_OPAQUE_INDIRECT_DRAW_CMDS_COUNT_UAV[0], 0);
        InterlockedExchange(MESH_CULL_TRANSP_INDIRECT_DRAW_CMDS_COUNT_UAV[0], 0);
    }

    AllMemoryBarrier();

    const uint instID = DTid.x;

    if (instID >= MESH_CULLING_REGISTRY.INST_COUNT) {
        return;
    }

    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[instID];
    const COMMON_MESH_INFO meshInfo = COMMON_MESH_INFOS[instInfo.MESH_IDX];

    const COMMON_MATERIAL material = COMMON_MATERIALS[instInfo.MATERIAL_IDX];

    // TODO: make 3 queues: opaque, alpha kill and transparent
    // For now only opaque objects are supported
    const bool isTranspMtl = IS_COMMON_MATERIAL_ALPHA_KILL(material) || IS_COMMON_MATERIAL_ALPHA_BLEND(material);

#ifdef ENV_DEBUG
    if (!COMMON_DBG_IS_MESH_GPU_CULLING_ENABLED) {
        PushCmd(meshInfo.FIRST_INDEX, meshInfo.INDEX_COUNT, meshInfo.FIRST_VERTEX, 0, 1, instID, isTranspMtl);
        return;
    }
#endif

    if (!IsInstVisible(instInfo)) {
        return;
    }

    PushCmd(meshInfo.FIRST_INDEX, meshInfo.INDEX_COUNT, meshInfo.FIRST_VERTEX, 0, 1, instID, isTranspMtl);
}