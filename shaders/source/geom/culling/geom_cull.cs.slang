#include "registers/common_registers.slang"
#include "registers/geom_culling_registers.slang"

#include "common/math/frustum_culling.slang"

#define HZB_CULLING_HZB_MIP_0_SIZE  GEOM_CULLING_HZB_MIP_0_SIZE
#define HZB_CULLING_HZB_MIPS_COUNT  GEOM_CULLING_HZB_MIPS_COUNT
#define HZB_CULLING_VIEW_PROJ_MATR  GEOM_CULLING_VIEW_PROJ_MATRIX
#define HZB_CULLING_VIEW_NEAR_PLANE GEOM_CULLING_FRUSTUM.GetNearPlane()
#include "common/math/hzb_culling.slang"


bool GeomVisCull(in GPU_AABB aabb)
{
    if (GEOM_CULLING_IS_FRUSTUM_CULLING_ENABLED && !Frustum_Visibility(GEOM_CULLING_FRUSTUM, aabb)) {
        return false;
    }

    if (GEOM_CULLING_IS_HZB_CULLING_ENABLED && !HZB_Visibility(aabb, GEOM_CULL_HZB)) {
        return false;
    }

    return true;
}


GPU_GeomSortKey GetGeomSortKey(in GPU_GeomInst inst)
{
    const float depth = -mul(GEOM_CULLING_VIEW_MATRIX, float4(inst.aabbWCS.GetCenter(), 1.f)).z;

    uint lodID;

    if (COMMON_DBG_FORCED_GEOM_LOD >= 0) {
        lodID = COMMON_DBG_FORCED_GEOM_LOD;
    } else {
        // Calculate fair LOD
        lodID = 0;
    }

    lodID = clamp(lodID, 0, COMMON_MESH_BUFFER[inst.meshID].lodCount - 1);

    const GPU_GeomMaterial material = COMMON_MATERIALS[inst.materialID];

    return GPU_GeomSortKey(material.type, inst.materialID, inst.meshID, lodID, depth);
}


groupshared uint GROUP_CULL_VIS_INST_COUNTER;
groupshared uint GROUP_CULL_VIS_INST_OFFSET;


[numthreads(GEOM_CULL_CS_GROUP_SIZE, 1, 1)]
[shader("compute")]
void main(in uint Gid : SV_GroupID, in uint GI : SV_GroupIndex, in uint DTid : SV_DispatchThreadID)
{
    if (GI == 0) {
        GROUP_CULL_VIS_INST_COUNTER = 0;
        GROUP_CULL_VIS_INST_OFFSET = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    const uint instID = DTid;

    GPU_GeomSortKey sortKey;
    GPU_GeomInst inst;

    bool isVisible = false;

    if (instID < COMMON_INST_COUNT) {
        inst = COMMON_INST_BUFFER[instID];
        isVisible = GeomVisCull(inst.aabbWCS);
    }

    uint groupLocalIndex = 0;

    if (isVisible) {
        sortKey = GetGeomSortKey(inst);

        InterlockedAdd(GROUP_CULL_VIS_INST_COUNTER, 1, groupLocalIndex);
    }

    GroupMemoryBarrierWithGroupSync();

    if (GI == 0 && GROUP_CULL_VIS_INST_COUNTER > 0) { ;
        InterlockedAdd(GEOM_CULL_VIS_INST_COUNTER_UAV[0], GROUP_CULL_VIS_INST_COUNTER, GROUP_CULL_VIS_INST_OFFSET);
    }

    GroupMemoryBarrierWithGroupSync();

    if (isVisible) {
        const uint index = GROUP_CULL_VIS_INST_OFFSET + groupLocalIndex;

        GEOM_CULL_VIS_SORT_KEYS_UAV[index] = sortKey;
        GEOM_CULL_VIS_GEOM_IDS_UAV[index] = instID;
    }
}