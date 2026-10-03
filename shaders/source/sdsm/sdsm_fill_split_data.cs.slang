#include "registers/sdsm_registers.slang"

#include "common/math/common_matr.slang"


float GetSdsmSplitZBound(in uint split)
{
    const float2 zRange = SDSM_VIEW_Z_RANGE[0];

    return zRange.x * pow(zRange.y / max(zRange.x, M3D_EPS), (float)split / CSM_SPLIT_COUNT);
}


float2 GetSdsmSplitZBounds(in uint split)
{
    return float2(GetSdsmSplitZBound(split), GetSdsmSplitZBound(split + 1)); 
}


[shader("compute")]
[numthreads(CSM_SPLIT_COUNT, 1, 1)]
void main(SV_GROUP_THREAD_FLAT_ID(groupThreadFlatID))
{
    const uint split = groupThreadFlatID;

    GPU_CsmSplitData splitData = SDSM_SPLITS[split][0];

    if (SDSM_PASS == GPU_SdsmPass::PREPARE_SPLITS) {
        // Previous-frame state
        splitData.camData.viewProjMatrPrev = splitData.camData.viewProjMatr;
        splitData.camData.invViewProjMatrPrev = splitData.camData.invViewProjMatr;

        // Current-frame view data
        splitData.zBounds = GetSdsmSplitZBounds(split);

        const float splitDepth = (splitData.zBounds.x + splitData.zBounds.y) * 0.5f;
        const float3 splitCenter = COMMON_MAIN_CAM_WPOS + COMMON_MAIN_CAM_VIEW_DIR * splitDepth;

        // TODO: Add stabilization
        splitData.camData.wPos = splitCenter - COMMON_SUN_DATA.direction * COMMON_SUN_DATA.distance;

        const float DdotU = abs(dot(COMMON_SUN_DATA.direction, M3D_AXIS_Y));
        const float3 up = IS_EQUAL(DdotU, 1.f) ? M3D_AXIS_Z : M3D_AXIS_Y;

        splitData.camData.viewMatr = LookAtRH(splitData.camData.wPos, splitCenter, up);
        splitData.camData.invViewMatr = Inverse(splitData.camData.viewMatr);

        // Reset bounds for the subsequent tightening pass.
        SDSM_SPLIT_VIEW_BOUNDS[split][0] = GPU_SdsmSplitViewBounds();
    } else if (SDSM_PASS == GPU_SdsmPass::FILL_SPLITS_PROJ_DATA) {
        const GPU_SBox splitBounds = SDSM_SPLIT_VIEW_BOUNDS[split][0].ToSBox();

        splitData.camData.zNear = splitBounds.minZ;
        splitData.camData.zFar = splitBounds.maxZ;

        splitData.camData.projMatr = OrthoRH_ZO(
            splitBounds.minX, splitBounds.maxX,
            splitBounds.minY, splitBounds.maxY,
            splitBounds.minZ, splitBounds.maxZ
        );
        splitData.camData.invProjMatr = Inverse(splitData.camData.projMatr);

        splitData.camData.viewProjMatr = mul(splitData.camData.projMatr, splitData.camData.viewMatr);
        splitData.camData.invViewProjMatr = Inverse(splitData.camData.viewProjMatr);

        splitData.camData.frustum.Construct(splitData.camData.viewProjMatr);

        const float orthoSize = Max(splitBounds.deltaXY);
        splitData.worldUnitsPerPixel = orthoSize / Max(splitData.rtSize);
    }

    SDSM_SPLITS[split][0] = splitData;
}