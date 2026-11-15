#include "registers/sdsm_registers.slang"

#include "common/math/common_matr.slang"


// Minimal size (world units) of a split's light-space bounds on each axis, so its projection never becomes singular
static const float SDSM_MIN_SPLIT_EXTENT = 0.01f;


float GetSdsmSplitZBound(in uint split)
{
    float2 zRange = SDSM_VIEW_Z_RANGE[0];

    if (zRange.x > zRange.y) {
        zRange = float2(COMMON_MAIN_CAM_Z_NEAR, COMMON_MAIN_CAM_Z_FAR);
    }

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

    GPU_CsmSplitData splitData = SDSM_SPLIT(split);

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
        SDSM_SPLIT_BOUNDS(split) = GPU_SdsmSplitViewBounds();
    } else if (SDSM_PASS == GPU_SdsmPass::FILL_SPLITS_PROJ_DATA) {
        const GPU_SdsmSplitViewBounds splitBounds = SDSM_SPLIT_BOUNDS(split);

        float3 boundsMin = splitBounds.isValid ? splitBounds.minXYZ : -ONEF3;
        float3 boundsMax = splitBounds.isValid ? splitBounds.maxXYZ : ONEF3;

        const float3 padding = max(FLOAT3(SDSM_MIN_SPLIT_EXTENT) - (boundsMax - boundsMin), ZEROF3) * 0.5f;
        boundsMin -= padding;
        boundsMax += padding;

        const float zNear = max(boundsMin.z - SDSM_SPLIT_Z_EXTNESION, M3D_EPS);
        const float zFar  = max(boundsMax.z + SDSM_SPLIT_Z_EXTNESION, zNear + SDSM_MIN_SPLIT_EXTENT);

        splitData.camData.zNear = zNear;
        splitData.camData.zFar = zFar;

        const float2 ext = (boundsMax.xy - boundsMin.xy) * SDSM_SPLIT_XY_EXTNESION_COEF;

        const float left   = boundsMin.x - ext.x;
        const float right  = boundsMax.x + ext.x;
        const float bottom = boundsMin.y - ext.y;
        const float top    = boundsMax.y + ext.y;

        splitData.camData.projMatr = OrthoRH_ZO(left, right, bottom, top, zNear, zFar);
        splitData.camData.invProjMatr = Inverse(splitData.camData.projMatr);

        splitData.camData.viewProjMatr = mul(splitData.camData.projMatr, splitData.camData.viewMatr);
        splitData.camData.invViewProjMatr = Inverse(splitData.camData.viewProjMatr);

        splitData.camData.frustum.Construct(splitData.camData.viewProjMatr);

        splitData.worldUnitsPerPixel = float2(right - left, top - bottom) / splitData.rtSize;
    }

    SDSM_SPLIT(split) = splitData;
}