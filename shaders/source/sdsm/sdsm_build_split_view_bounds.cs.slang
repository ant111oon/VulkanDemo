#include "registers/sdsm_registers.slang"

#include "common/common_funcs.slang"
#include "common/math/bits.slang"


uint GetSdsmSplit(in float viewZ)
{
    for (uint split = 0; split < CSM_SPLIT_COUNT; ++split) {
        const float2 zBounds = SDSM_SPLITS[split][0].zBounds;

        if (viewZ >= zBounds.x && viewZ < zBounds.y) {
            return split;
        }
    }

    return CSM_SPLIT_COUNT;
}


[shader("compute")]
[numthreads(SDSM_FILL_SPLIT_BOUNDS_CS_GROUP_SIZE_X, SDSM_FILL_SPLIT_BOUNDS_CS_GROUP_SIZE_X, 1)]
void main(SV_GROUP_THREAD_ID(groupThreadID), SV_DISPATCH_THREAD_ID(dispatchThreadID))
{
    const uint2 pixCoord = dispatchThreadID.xy;

    if (any(pixCoord >= COMMON_SCREEN_SIZE)) {
        return;
    }

    const float depth = LOAD_TEX(SDSM_DEPTH, int3(pixCoord, 0)).r;

    if (IsBackgroundDepth(depth)) {
        return;
    }

    const float2 uv = PixelCoordToUV(pixCoord, COMMON_SCREEN_SIZE);
    const float3 wpos = WorldPosFromDepth(uv, depth, COMMON_MAIN_CAM_INV_VIEW_PROJ_MATRIX);

    const float3 vpos = mul(COMMON_MAIN_CAM_VIEW_MATRIX, float4(wpos, 1.f)).xyz;
    const float viewZ = -vpos.z;

    const uint split = GetSdsmSplit(viewZ);
    
    if (split >= CSM_SPLIT_COUNT) {
        return;
    }

    const float4x4 splitCamViewMatr = SDSM_SPLITS[split][0].camData.viewMatr;

    float3 lightSpaceVPos = mul(splitCamViewMatr, float4(wpos, 1.f)).xyz;
    lightSpaceVPos.z = -lightSpaceVPos.z;

    SDSM_SPLIT_VIEW_BOUNDS[split][0].Update(lightSpaceVPos);
}