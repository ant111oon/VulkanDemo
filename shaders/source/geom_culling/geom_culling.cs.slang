#include "registers/common_registers.slang"
#include "registers/mesh_culling_registers.slang"

#include "common/common_structs.slang"

#define FRUSTUM_CULLING_VIEW_PROJ_MATR COMMON_CB.CULLING_VIEW_PROJ_MATRIX
#include "common/math/frustum_culling.slang"

#define HZB_CULLING_HZB_TEX         COMMON_HZB
#define HZB_CULLING_HZB_TEX_SAMPLER GET_COMMON_SAMPLER(COMMON_SAMPLER_IDX::NEAREST_CLAMP_TO_EDGE)
#define HZB_CULLING_HZB_MIP_0_SIZE  COMMON_CB.SCREEN_SIZE
#define HZB_CULLING_VIEW_PROJ_MATR  FRUSTUM_CULLING_VIEW_PROJ_MATR
#define HZB_CULLING_HZB_MIPS_COUNT  COMMON_CB.HZB_MIPS_COUNT
#include "common/math/hzb_culling.slang"

#include "common/math/contribution_culling.slang"


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


AABB GetInstAABB(in uint aabbIdx)
{
    return COMMON_AABB_BUFFER[aabbIdx].GetAABB();
}


bool Pass_Visibility(in COMMON_INST_DATA inst)
{
    const bool isFrustumCullEnabled = COMMON_DBG_IS_MESH_GPU_FRUSTUM_CULLING_ENABLED;
    const bool isContribCullEnabled = COMMON_DBG_IS_MESH_GPU_CONTRIBUTION_CULLING_ENABLED;
    const bool isHZBCullEnabled = COMMON_DBG_IS_MESH_GPU_HZB_CULLING_ENABLED;

    if (!isFrustumCullEnabled && !isContribCullEnabled && !isHZBCullEnabled) {
        return true;
    }

    AABB aabb = GetInstAABB(inst.VOLUME_IDX);

    if (isFrustumCullEnabled) {
        if (!Frustum_Visibility(COMMON_CULLING_CAMERA_FRUSTUM, aabb)) {
            return false;
        }
    }

    if (!isContribCullEnabled && !isHZBCullEnabled) {
        return true;
    }

    SBOX sbox = aabb.GetSBOX(HZB_CULLING_VIEW_PROJ_MATR);

    sbox.minUV = saturate(sbox.minUV);
    sbox.maxUV = saturate(sbox.maxUV);
    sbox.closestZ = saturate(sbox.closestZ);

    if (isContribCullEnabled) {
        if (!Contribution_Visibility(sbox, COMMON_SCREEN_SIZE, FLOAT2(COMMON_VIS_CONTRIBUTION_FALLOFF))) {
            return false;
        }
    }

    if (isHZBCullEnabled) {
        if (!HZB_Visibility(sbox)) {
            return false;
        }
    }
    
    return true; 
}


[numthreads(64, 1, 1)]
[shader("compute")]
void main(uint DTid : SV_DispatchThreadID)
{
    const uint instID = DTid;

    if (instID >= PUSH_CONSTS.INST_COUNT) {
        return;
    }

    const COMMON_INST_DATA inst = COMMON_INST_DATA_BUFFER[instID];

    if (!Pass_Visibility(inst)) {
        return;
    }

    PushInstToCmdQueue(inst, instID);
}