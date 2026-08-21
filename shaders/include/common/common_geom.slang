#ifndef COMMON_GEOM_H
#define COMMON_GEOM_H

#include "common/math/bits.slang"
#include "common/math/geom_primitives.slang"


enum GPU_GeomStreamID : uint
{
    POSITION,
    NORMAL,
    UV,
    TANGENT,
    COUNT
};


enum GPU_GeomMatType : uint
{
    OPAQUE,
    AKILL,
    COUNT
};


struct GPU_GeomMaterial
{
    property bool isOpaque
    {
        get { return type == GPU_GeomMatType::OPAQUE; }
    }

    property bool isAlphaKill
    {
        get { return type == GPU_GeomMatType::AKILL; }
    }

    property bool isDoubleSided
    {
        get { return (bool)doubleSided; }
    }

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

    GPU_GeomMatType type;

    private uint doubleSided : 1;
    
    private uint padding_0 : 31;
    private uint padding_1;
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
        get { return float3(unpackHalf2x16ToFloat(aabbMinMaxLCS.x), unpackHalf2x16ToFloat(aabbMinMaxLCS.y).x); }
    }

    property float3 maxPosLCS
    {
        get { return float3(unpackHalf2x16ToFloat(aabbMinMaxLCS.y).y, unpackHalf2x16ToFloat(aabbMinMaxLCS.z)); }
    }

    property GPU_AABB aabbLCS
    {
        get { return GPU_AABB(minPosLCS, maxPosLCS); }
    }

    uint firstVertex;
    uint vertexCount;

    uint firstLOD; // Index of mesh LOD 0 inside LOD buffer
    uint lodCount;

    private uint3 aabbMinMaxLCS; // x - MIN.xy, y - MIN.z and MAX.x, z - MAX.yz
    private uint padding;
};


struct GPU_GeomInst
{
    __init()
    {
        matrWCS = M3D_IDENTITY_3X4;
        aabbMinMaxWCS = ZEROU3;
        meshID = 0u;
        materialID = 0u;
    }

    property float3 minPosWCS
    {
        get { return float3(unpackHalf2x16ToFloat(aabbMinMaxWCS.x), unpackHalf2x16ToFloat(aabbMinMaxWCS.y).x); }
    }

    property float3 maxPosWCS
    {
        get { return float3(unpackHalf2x16ToFloat(aabbMinMaxWCS.y).y, unpackHalf2x16ToFloat(aabbMinMaxWCS.z)); }
    }

    property GPU_AABB aabbWCS
    {
        get { return GPU_AABB(minPosWCS, maxPosWCS); }
    }

    row_major float3x4 matrWCS;

    private uint3 aabbMinMaxWCS; // x - MIN.xy, y - MIN.z and MAX.x, z - MAX.yz
    uint meshID : 16;
    uint materialID : 16;
};


struct GPU_GeomSortKey
{
    __init()
    {
        key = ZEROU2;
    }

    __init(in GPU_GeomMatType matType, in uint matID, in uint meshID, in uint lodID, in float depth)
    {
        Construct(matType, matID, meshID, lodID, depth);
    }

    [mutating]
    void Construct(in GPU_GeomMatType matType, in uint materialID, in uint meshID, in uint lodID, in float depth)
    {
        static_assert(GPU_GeomMatType::COUNT < (1 << GEOM_SORT_MAT_TYPE_BITS), "GPU_GeomMatType is out of bit bounds");

        key = ZEROU2;

        this.depth = depth;
        this.lodID = lodID;
        this.meshID = meshID;
        this.materialID = materialID;
        this.matType = matType;
    }

    uint GetDigits(in uint offset, in uint bits)
    {
        if (offset >= 32) {
            return bitfieldExtract(key.y, offset - 32, bits);
        }

        if (offset + bits <= 32) {
            return bitfieldExtract(key.x, offset, bits);
        }

        // Digit crosses from key.x into key.y
        const uint loBits = 32 - offset;
        const uint hiBits = bits - loBits;

        const uint lo = bitfieldExtract(key.x, offset, loBits);
        const uint hi = bitfieldExtract(key.y, 0, hiBits);

        return lo | (hi << loBits);
    }

    property GPU_GeomMatType matType
    {
        set { key.y = bitfieldInsert(key.y, (uint)newValue, GEOM_SORT_MAT_TYPE_OFFSET, GEOM_SORT_MAT_TYPE_BITS); }
        get { return GPU_GeomMatType(bitfieldExtract(key.y, GEOM_SORT_MAT_TYPE_OFFSET, GEOM_SORT_MAT_TYPE_BITS)); }
    }

    property uint materialID
    {
        set { key.y = bitfieldInsert(key.y, newValue, GEOM_SORT_MAT_ID_OFFSET, GEOM_SORT_MAT_ID_BITS); }
        get { return bitfieldExtract(key.y, GEOM_SORT_MAT_ID_OFFSET, GEOM_SORT_MAT_ID_BITS); }
    }

    property uint meshID
    {
        set { key.y = bitfieldInsert(key.y, newValue, GEOM_SORT_MESH_OFFSET, GEOM_SORT_MESH_BITS); }
        get { return bitfieldExtract(key.y, GEOM_SORT_MESH_OFFSET, GEOM_SORT_MESH_BITS); }
    }

    property uint lodID
    {
        set { key.y = bitfieldInsert(key.y, newValue, GEOM_SORT_LOD_OFFSET, GEOM_SORT_LOD_BITS); }
        get { return bitfieldExtract(key.y, GEOM_SORT_LOD_OFFSET, GEOM_SORT_LOD_BITS); }
    }

    property float depth
    {
        set { key.x = FloatToSortableUint(newValue); }
        get { return SortableUintToFloat(key.x); }
    }

    private static const uint GEOM_SORT_LOD_OFFSET = 0;
    private static const uint GEOM_SORT_LOD_BITS = 3;

    private static const uint GEOM_SORT_MESH_OFFSET = 3;
    private static const uint GEOM_SORT_MESH_BITS = 16;

    private static const uint GEOM_SORT_MAT_ID_OFFSET = 19;
    private static const uint GEOM_SORT_MAT_ID_BITS = 11;

    private static const uint GEOM_SORT_MAT_TYPE_OFFSET = 30;
    private static const uint GEOM_SORT_MAT_TYPE_BITS = 2;

    private uint2 key;
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