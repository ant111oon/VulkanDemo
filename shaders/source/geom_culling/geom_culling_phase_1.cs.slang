#define GEOM_CULLING_PHASE_1

#define GEOM_CULLING_FRUSTUM_ENABLED
#define GEOM_CULLING_CONTRIBUTION_ENABLED
#include "common/geom_culling_incl.slang"


[numthreads(GEOM_CULLING_CS_GPOUP_SIZE, 1, 1)]
[shader("compute")]
void main(uint DTid : SV_DispatchThreadID)
{
    const uint instID = DTid;

    if (instID >= GEOM_CULLING_INST_COUNT) {
        return;
    }

    if (!GeomCullingTestInstanceVisibilityFlag(instID)) {
        return;
    }

    const COMMON_INST inst = COMMON_INST_BUFFER[instID];
    const AABB aabb = COMMON_INST_AABB_BUFFER[inst.VOLUME_IDX].GetAABB();

    if (GeomCullingVisibility(aabb)) {
        GeomCullingPushInstToCmdQueue(instID, inst, aabb.DistanceTo(COMMON_CAM_WPOS));
    }
}