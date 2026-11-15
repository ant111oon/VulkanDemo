#ifndef SDSM_REGISTERS_H
#define SDSM_REGISTERS_H

#include "common_registers.slang"

#include "common/common_lighting_structs.slang"


static const uint SDSM_FILL_SPLIT_BOUNDS_CS_GROUP_SIZE_X = 32;


struct GPU_SdsmSplitViewBounds
{
    __init()
    {
        minXYZUint = UINT3(MIN_INIT);
        maxXYZUint = UINT3(MAX_INIT);
    }

    [mutating]
    void Update<bool isAtomic>(in float3 xyz)
    {
        const uint3 xyzUint = FloatToSortableUint(xyz);

        if (isAtomic) {
            InterlockedMin(minXYZUint.x, xyzUint.x);
            InterlockedMin(minXYZUint.y, xyzUint.y);
            InterlockedMin(minXYZUint.z, xyzUint.z);
    
            InterlockedMax(maxXYZUint.x, xyzUint.x);
            InterlockedMax(maxXYZUint.y, xyzUint.y);
            InterlockedMax(maxXYZUint.z, xyzUint.z);
        } else {
            minXYZUint.x = min(minXYZUint.x, xyzUint.x);
            minXYZUint.y = min(minXYZUint.y, xyzUint.y);
            minXYZUint.z = min(minXYZUint.z, xyzUint.z);

            maxXYZUint.x = max(maxXYZUint.x, xyzUint.x);
            maxXYZUint.y = max(maxXYZUint.y, xyzUint.y);
            maxXYZUint.z = max(maxXYZUint.z, xyzUint.z);
        }
    }

    [mutating]
    void Update<bool isAtomic>(in float3 minXYZ, in float3 maxXYZ)
    {
        Update<isAtomic>(minXYZ);
        Update<isAtomic>(maxXYZ);
    }

    GPU_SBox ToSBox()
    {
        return GPU_SBox(minXYZ, maxXYZ);
    }

    property float3 minXYZ
    {
        get { return SortableUintToFloat(minXYZUint); }
    }

    property float3 maxXYZ
    {
        get { return SortableUintToFloat(maxXYZUint); }
    }

    property bool isValid
    {
        get { return all(minXYZ <= maxXYZ); }
    }

    private static const uint MIN_INIT = FloatToSortableUint(M3D_FLT_MAX);
    private static const uint MAX_INIT = FloatToSortableUint(M3D_FLT_LOWEST);

    private uint3 minXYZUint;
    private uint3 maxXYZUint;
};


enum GPU_SdsmPass
{
    PREPARE_SPLITS,
    BUILD_VIEW_BOUNDS,
    FILL_SPLITS_PROJ_DATA,
};

struct GPU_SdsmPushConst
{
    GPU_SdsmPass pass;
    float splitZExtension;
    float splitXYExtensionCoef;
    uint2 splitRTSize[CSM_SPLIT_COUNT];
};

[[vk::push_constant]] GPU_SdsmPushConst SDSM_CONST;

#define SDSM_PASS                     SDSM_CONST.pass
#define SDSM_SPLIT_Z_EXTNESION        SDSM_CONST.splitZExtension
#define SDSM_SPLIT_XY_EXTNESION_COEF  SDSM_CONST.splitXYExtensionCoef
#define SDSM_SPLIT_RT_SIZE(SPLIT_IDX) SDSM_CONST.splitRTSize[SPLIT_IDX]


[vk::binding(0, DESC_SET_PER_DRAW)] Texture2D SDSM_DEPTH;

[vk::binding(1, DESC_SET_PER_DRAW)] StructuredBuffer<float2> SDSM_VIEW_Z_RANGE;
[vk::binding(2, DESC_SET_PER_DRAW)] RWStructuredBuffer<GPU_SdsmSplitViewBounds, ScalarDataLayout> SDSM_SPLIT_VIEW_BOUNDS[CSM_SPLIT_COUNT];

[vk::binding(3, DESC_SET_PER_DRAW)] RWStructuredBuffer<GPU_CsmSplitData> SDSM_SPLITS[CSM_SPLIT_COUNT];

// The split index comes from per-pixel depth (or per-thread split ID), so it can differ within a wave.
#define SDSM_SPLIT(SPLIT_IDX)        SDSM_SPLITS[NonUniformResourceIndex((uint)(SPLIT_IDX))][0]
#define SDSM_SPLIT_BOUNDS(SPLIT_IDX) SDSM_SPLIT_VIEW_BOUNDS[NonUniformResourceIndex((uint)(SPLIT_IDX))][0]

#endif