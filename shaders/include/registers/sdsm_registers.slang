#ifndef SDSM_REGISTERS_H
#define SDSM_REGISTERS_H

#include "common_registers.slang"

#include "common/common_lighting_structs.slang"


static const uint SDSM_FILL_SPLIT_BOUNDS_CS_GROUP_SIZE_X = 32;


struct GPU_SdsmSplitViewBounds
{
    __init()
    {
        static const uint minInit = FloatToSortableUint(M3D_FLT_MAX);
        static const uint maxInit = FloatToSortableUint(M3D_FLT_LOWEST);

        minXYZUint = UINT3(minInit);
        maxXYZUint = UINT3(maxInit);
    }

    [mutating]
    void Update(in float3 xyz)
    {
        const uint3 xyzUint = FloatToSortableUint(xyz);

        InterlockedMin(minXYZUint.x, xyzUint.x);
        InterlockedMin(minXYZUint.y, xyzUint.y);
        InterlockedMin(minXYZUint.z, xyzUint.z);

        InterlockedMax(maxXYZUint.x, xyzUint.x);
        InterlockedMax(maxXYZUint.y, xyzUint.y);
        InterlockedMax(maxXYZUint.z, xyzUint.z);
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
};

[[vk::push_constant]] GPU_SdsmPushConst SDSM_CONST;

#define SDSM_PASS SDSM_CONST.pass


[vk::binding(0, DESC_SET_PER_DRAW)] Texture2D SDSM_DEPTH;

[vk::binding(1, DESC_SET_PER_DRAW)] StructuredBuffer<float2> SDSM_VIEW_Z_RANGE;
[vk::binding(2, DESC_SET_PER_DRAW)] RWStructuredBuffer<GPU_SdsmSplitViewBounds, ScalarDataLayout> SDSM_SPLIT_VIEW_BOUNDS[CSM_SPLIT_COUNT];

[vk::binding(3, DESC_SET_PER_DRAW)] RWStructuredBuffer<GPU_CsmSplitData> SDSM_SPLITS[CSM_SPLIT_COUNT];

#endif