#define GEOM_CULLING_OCCLUDERS_PASS

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

    if (!GeomCullingWasInstVisibleInPrevPass(instID)) {
        return;
    }

    const COMMON_INST_DATA inst = COMMON_INST_DATA_BUFFER[instID];

    if (GeomCullingVisibility(inst)) {
        GeomCullingPushInstToCmdQueue(inst, instID);
    }
}