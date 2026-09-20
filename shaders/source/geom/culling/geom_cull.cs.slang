#include "registers/common_registers.slang"
#include "registers/geom_cull_registers.slang"

#include "common/math/frustum_culling.slang"

#define HZB_CULLING_HZB_MIP_0_SIZE       GEOM_CULL_HZB_MIP_0_SIZE
#define HZB_CULLING_HZB_MIPS_COUNT       GEOM_CULL_HZB_MIPS_COUNT
#define HZB_CULLING_VIEW_PROJ_MATR_PREV  GEOM_CULL_VIEW_PROJ_MATR_PREV
#define HZB_CULLING_VIEW_NEAR_PLANE      GEOM_CULL_FRUSTUM.GetNearPlane()
#include "common/math/hzb_culling.slang"


static const float GEOM_LOD_THRESHOLDS[GEOM_MAX_LOD_COUNT] =
{
    0.50f,
    0.35f,
    0.25f,
    0.15f,
    0.05f,
    0.03f
};


uint GeomSelectLOD(float sizePixels, in float lodThresholds[GEOM_MAX_LOD_COUNT])
{
    for (uint lodID = 0; lodID < GEOM_MAX_LOD_COUNT; ++lodID) {
        if (sizePixels >= lodThresholds[lodID]) {
            return lodID;
        }
    }

    return GEOM_MAX_LOD_COUNT - 1;
}


GPU_GeomSortKey GetGeomSortKey(in GPU_GeomInst inst, in GPU_SBox sbox)
{
    const float depth = -mul(GEOM_CULL_VIEW_MATR, float4(inst.aabbWCS.GetCenter(), 1.f)).z;

    uint lodID;

    if (COMMON_DBG_FORCED_GEOM_LOD >= 0) {
        lodID = COMMON_DBG_FORCED_GEOM_LOD;
    } else {
        // const float coverage = Max(sbox.maxUV - sbox.minUV);
        // lodID = GeomSelectLOD(coverage, GEOM_LOD_THRESHOLDS);
        lodID = 0;
    }

    lodID = clamp(lodID, 0, COMMON_MESH_BUFFER[inst.meshID].lodCount - 1);

    const GPU_GeomMaterial material = COMMON_MATERIALS[inst.materialID];

    return GPU_GeomSortKey(material.type, inst.materialID, inst.meshID, lodID, depth);
}


bool GeomCullVisibility(in GPU_GeomInst inst, out GPU_GeomSortKey sortKey)
{
    sortKey = GPU_GeomSortKey();
    
    const GPU_AABB aabb = inst.aabbWCS;

    if (GEOM_CULL_IS_FRUSTUM_CULLING_ENABLED && !Frustum_Visibility(GEOM_CULL_FRUSTUM, aabb)) {
        return false;
    }

    GPU_SBox sbox;

    if (GEOM_CULL_IS_HZB_CULLING_ENABLED && !HZB_Visibility(aabb, GEOM_CULL_HZB, sbox)) {
        return false;
    }

    sortKey = GetGeomSortKey(inst, sbox);

    return true;
}


groupshared uint GROUP_CULL_VIS_INST_COUNTER;
groupshared uint GROUP_CULL_VIS_INST_OFFSET;


[numthreads(GEOM_CULL_CS_GROUP_SIZE, 1, 1)]
[shader("compute")]
void main(SV_GROUP_THREAD_FLAT_ID(groupThreadID), SV_DISPATCH_THREAD_ID(dispatchThreadID))
{
    if (groupThreadID == 0) {
        GROUP_CULL_VIS_INST_COUNTER = 0;
        GROUP_CULL_VIS_INST_OFFSET = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    const uint instID = dispatchThreadID.x;

    GPU_GeomSortKey sortKey;
    GPU_GeomInst inst;

    bool isVisible = false;

    if (instID < COMMON_INST_COUNT) {
        inst = COMMON_INST_BUFFER[instID];
        isVisible = GeomCullVisibility(inst, sortKey);
    }

    uint groupLocalIndex = 0;

    if (isVisible) {
        InterlockedAdd(GROUP_CULL_VIS_INST_COUNTER, 1, groupLocalIndex);
    }

    GroupMemoryBarrierWithGroupSync();

    if (groupThreadID == 0 && GROUP_CULL_VIS_INST_COUNTER > 0) { ;
        InterlockedAdd(GEOM_CULL_VIS_INST_COUNTER[0], GROUP_CULL_VIS_INST_COUNTER, GROUP_CULL_VIS_INST_OFFSET);
    }

    GroupMemoryBarrierWithGroupSync();

    if (isVisible) {
        const uint index = GROUP_CULL_VIS_INST_OFFSET + groupLocalIndex;

        GEOM_CULL_VIS_INST_SORT_KEYS[index] = sortKey;
        GEOM_CULL_VIS_INST_IDS[index] = instID;
    }
}