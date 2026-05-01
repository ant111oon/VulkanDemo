#define GEOM_CULLING_OCCLUSION_PASS

#define GEOM_CULLING_FRUSTUM_ENABLED
#define GEOM_CULLING_CONTRIBUTION_ENABLED
#define GEOM_CULLING_HZB_ENABLED
#include "common/geom_culling_incl.slang"


[numthreads(GEOM_CULLING_CS_GPOUP_SIZE, 1, 1)]
[shader("compute")]
void main(uint DTid : SV_DispatchThreadID)
{
    const uint instID = DTid;

    if (instID >= GEOM_CULLING_INST_COUNT) {
        return;
    }

    const COMMON_INST_DATA inst = COMMON_INST_DATA_BUFFER[instID];

    const bool visible = GeomCullingVisibility(inst);

    // Only object that is visible in the current frame
    // and was not drawn in the first pass should be drawn in the second pass
    const bool shouldDraw = visible && !GeomCullingWasInstVisibleInPrevPass(instID);

    if (shouldDraw) {
        GeomCullingPushInstToCmdQueue(inst, instID);
    }

    GeomCullingWriteVisFlag(instID, visible);
}