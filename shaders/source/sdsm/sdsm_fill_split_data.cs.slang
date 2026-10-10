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

    // TODO: Add stabilization
    if (SDSM_PASS == GPU_SdsmPass::PREPARE_SPLITS) {
        // Previous-frame state
        splitData.camData.viewMatrPrev = splitData.camData.viewMatr;
        splitData.camData.projMatrPrev = splitData.camData.projMatr;
        splitData.camData.viewProjMatrPrev = splitData.camData.viewProjMatr;
        splitData.camData.invViewMatrPrev = splitData.camData.invViewMatr;
        splitData.camData.invProjMatrPrev = splitData.camData.invProjMatr;
        splitData.camData.invViewProjMatrPrev = splitData.camData.invViewProjMatr;

        // Current-frame view data
        splitData.zBounds = GetSdsmSplitZBounds(split);

        const float splitDepth = (splitData.zBounds.x + splitData.zBounds.y) * 0.5f;
        const float3 splitCenter = COMMON_MAIN_CAM_WPOS + COMMON_MAIN_CAM_VIEW_DIR * splitDepth;

        splitData.camData.wPos = splitCenter - COMMON_SUN_DATA.direction * (COMMON_SUN_DATA.distance + SDSM_SPLIT_Z_EXTNESION);

        const float DdotU = abs(dot(COMMON_SUN_DATA.direction, M3D_AXIS_Y));
        const float3 up = IS_EQUAL(DdotU, 1.f) ? M3D_AXIS_Z : M3D_AXIS_Y;

        splitData.camData.viewDir = normalize(COMMON_SUN_DATA.direction);

        const float3 r = normalize(cross(splitData.camData.viewDir, up));
        splitData.camData.upDir = cross(r, splitData.camData.viewDir);

        splitData.camData.viewMatr = LookAtRH(splitData.camData.wPos, splitCenter, up);
        splitData.camData.invViewMatr = Inverse(splitData.camData.viewMatr);

        splitData.rtSize = SDSM_SPLIT_RT_SIZE(split);

        // Reset bounds for the subsequent tightening pass.
        SDSM_SPLIT_VIEW_BOUNDS[split][0] = GPU_SdsmSplitViewBounds();
    } else if (SDSM_PASS == GPU_SdsmPass::FILL_SPLITS_PROJ_DATA) {
        const GPU_SBox splitBounds = SDSM_SPLIT_VIEW_BOUNDS[split][0].ToSBox();

        const float zNear = max(splitBounds.minZ - SDSM_SPLIT_Z_EXTNESION, M3D_EPS);
        const float zFar = splitBounds.maxZ + SDSM_SPLIT_Z_EXTNESION;

        splitData.camData.zNear = zNear;
        splitData.camData.zFar = zFar;

        const float2 ext = splitBounds.deltaXY * SDSM_SPLIT_XY_EXTNESION_COEF;

        const float left   = splitBounds.minX - ext.x;
        const float right  = splitBounds.maxX + ext.x;
        const float bottom = splitBounds.minY - ext.y;
        const float top    = splitBounds.maxY + ext.y;

        splitData.camData.projMatr = OrthoRH_ZO(left, right, bottom, top, zNear, zFar);
        splitData.camData.invProjMatr = Inverse(splitData.camData.projMatr);

        splitData.camData.viewProjMatr = mul(splitData.camData.projMatr, splitData.camData.viewMatr);
        splitData.camData.invViewProjMatr = Inverse(splitData.camData.viewProjMatr);

        splitData.camData.frustum.Construct(splitData.camData.viewProjMatr);

        const float orthoSize = Max(splitBounds.deltaXY);
        splitData.worldUnitsPerPixel = orthoSize / Max(splitData.rtSize);
    }

    SDSM_SPLITS[split][0] = splitData;
}