#ifndef COMMON_GEOM_H
#define COMMON_GEOM_H

#include "common/math/bits.slang"
#include "common/math/geom_primitives.slang"


enum GPU_GeomStreamID
{
    POSITION,
    NORMAL,
    UV,
    TANGENT,
    COUNT
};


struct GPU_GeomMaterial
{
    bool IsOpaque() { return (bool)isOpaque; }
    bool IsAlphaKill() { return (bool)isAKill; }

    bool IsDoubleSided() { return (bool)isDoubleSided; }

    float4 albedoMult;

    float3 emissiveMult;
    float  alphaRef;

    float normalScale;
    float metalnessScale;
    float roughnessScale;
    float aoCoef;

    int albedoTexID;
    int normalTexID;
    int metalRoughTexID;
    int aoTexID;

    int emissiveTexID;

    uint isOpaque : 1;
    uint isAKill : 1;
    uint isDoubleSided : 1;
    
    private uint padding_0 : 29;
    private uint2 padding_1;
};


struct GPU_MeshLOD
{
    uint firstIndex;
    uint indexCount;
};


struct GPU_Mesh
{
    property float3 minPosLCS
    {
        get { return float3(unpackHalf2x16ToFloat(aabbMinMaxLcsPacked.x), unpackHalf2x16ToFloat(aabbMinMaxLcsPacked.y).x); }
    }

    property float3 maxPosLCS
    {
        get { return float3(unpackHalf2x16ToFloat(aabbMinMaxLcsPacked.y).y, unpackHalf2x16ToFloat(aabbMinMaxLcsPacked.z)); }
    }

    property GPU_AABB aabbLCS
    {
        get { return GPU_AABB(minPosLCS, maxPosLCS); }
    }

    uint firstVertex;
    uint vertexCount;

    uint firstLOD; // Index of mesh LOD 0 inside LOD buffer
    uint lodCount;

    private uint3 aabbMinMaxLcsPacked; // x - MIN.xy, y - MIN.z and MAX.x, z - MAX.yz
    private uint padding;
};


struct GPU_GeomInst
{
    property float3 minPosWCS
    {
        get { return float3(unpackHalf2x16ToFloat(aabbMinMaxWcsPacked.x), unpackHalf2x16ToFloat(aabbMinMaxWcsPacked.y).x); }
    }

    property float3 maxPosWCS
    {
        get { return float3(unpackHalf2x16ToFloat(aabbMinMaxWcsPacked.y).y, unpackHalf2x16ToFloat(aabbMinMaxWcsPacked.z)); }
    }

    property GPU_AABB aabbWCS
    {
        get { return GPU_AABB(minPosWCS, maxPosWCS); }
    }

    row_major float3x4 matrWCS;

    private uint3 aabbMinMaxWcsPacked; // x - MIN.xy, y - MIN.z and MAX.x, z - MAX.yz
    uint meshID : 16;
    uint materialID : 16;
};


enum GPU_GeomQueue : uint
{
    OPAQUE,
    AKILL,
    COUNT
};


struct GPU_GeomBatch
{
    uint meshID;
    uint lodID;
    uint firstInst;
    uint instCount;
};


#define GEOM_QUEUE_OPAQUE (uint)GPU_GeomQueue::OPAQUE
#define GEOM_QUEUE_AKILL  (uint)GPU_GeomQueue::AKILL
#define GEOM_QUEUE_COUNT  (uint)GPU_GeomQueue::COUNT

static const uint GEOM_CULLING_CS_GROUP_SIZE = 1024;
static const uint GEOM_BATCH_CS_GROUP_SIZE = 1024;
static const uint GEOM_DRAW_CMD_GEN_CS_GROUP_SIZE = 512;


#endif