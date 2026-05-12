#ifndef GEOM_STREAM_INCL_H
#define GEOM_STREAM_INCL_H

#include "registers/common_registers.slang"


float3 GeomStreamLoadPosition(in uint vertID)
{
    ByteAddressBuffer stream = COMMON_GEOM_STREAMS[COMMON_GEOM_STREAM_ID::POSITION];

    const uint2 data = stream.Load<uint2>(vertID * sizeof(uint2));

    return float3(
        unpackHalf2x16ToFloat(data[0]),
        unpackHalf2x16ToFloat(data[1])[0]
    );
}


float3 GeomStreamLoadNormal(in uint vertID)
{
    ByteAddressBuffer stream = COMMON_GEOM_STREAMS[COMMON_GEOM_STREAM_ID::NORMAL];

    const float3 normal = unpackSnorm4x8ToFloat(
        stream.Load<uint>(vertID * sizeof(uint))
    ).xyz;

    return normalize(normal);
}


float2 GeomStreamLoadUV(in uint vertID)
{
    ByteAddressBuffer stream = COMMON_GEOM_STREAMS[COMMON_GEOM_STREAM_ID::UV];

    return unpackHalf2x16ToFloat(
        stream.Load<uint>(vertID * sizeof(uint))
    );
}


float4 GeomStreamLoadTangent(in uint vertID)
{
    ByteAddressBuffer stream = COMMON_GEOM_STREAMS[COMMON_GEOM_STREAM_ID::TANGENT];

    const float4 tangent = unpackSnorm4x8ToFloat(
        stream.Load<uint>(vertID * sizeof(uint))
    );

    return float4(normalize(tangent.xyz), sign(tangent.w));
}


#endif