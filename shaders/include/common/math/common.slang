#ifndef MATH_COMMON_H
#define MATH_COMMON_H

#include "constants.slang"


#define IS_FLAG_SET(VALUE, MASK) (((uint)(VALUE) & (uint)(MASK)) != 0u)
#define IS_BIT_SET(VALUE, BIT)   IS_FLAG_SET((uint)(VALUE), 1u << (uint)(BIT))

#define M3D_IS_ZERO(x)     IsZero(x)
#define M3D_IS_EQUAL(l, r) M3D_IS_ZERO((l) - (r))


bool IsZero(in float value)
{
    return abs(value) < M3D_EPS;
}


bool IsZero(in float2 vec)
{
    return IsZero(vec.x) && IsZero(vec.y);
}


bool IsZero(in float3 vec)
{
    return IsZero(vec.xy) && IsZero(vec.z);
}


bool IsZero(in float4 vec)
{
    return IsZero(vec.xyz) && IsZero(vec.w);
}


float Min(in float a, in float b)
{
    return min(a, b);
}


float Min(in float a, in float b, in float c)
{
    return min(min(a, b), c);
}


float Min(in float2 val)
{
    return min(val.x, val.y);
}


float Min(in float3 val)
{
    return min(min(val.x, val.y), val.z);
}


float Min(in float4 val)
{
    return min(min(val.x, val.y), min(val.z, val.w));
}


float Max(in float a, in float b)
{
    return max(a, b);
}


float Max(in float a, in float b, in float c)
{
    return max(max(a, b), c);
}


float Max(in float2 val)
{
    return max(val.x, val.y);
}


float Max(in float3 val)
{
    return max(max(val.x, val.y), val.z);
}


float Max(in float4 val)
{
    return max(max(val.x, val.y), max(val.z, val.w));
}


void CalcTangBitang(in float3 norm, out float3 tang, out float3 bitang)
{
    const float3 up = abs(norm.z) < 0.999f ? M3D_AXIS_Z : M3D_AXIS_X;
    
    tang = normalize(cross(up, norm));
    bitang = cross(norm, tang);
}


float3x3 CalcTBN(in float3 tang, in float3 norm)
{
    const float3 T = normalize(tang - dot(tang, norm) * norm);
    const float3 B = normalize(cross(norm, T));

    return float3x3(T, B, norm);
}


// Return depth in [near, far] range
float LinearizeDepth(in float depth, in float near, in float far)
{
#ifdef ENV_REVERSED_Z
    return (near * far) / (near + depth * (far - near));
#else
    return (near * far) / (far - depth * (far - near));
#endif
}


float LinearizeDepth01(in float depth, in float near, in float far)
{
    const float linear = LinearizeDepth(depth, near, far);
    return (linear - near) / (far - near);
}


float3 UVFaceToDir(in float2 uv01, in uint face)
{
    float2 uv = UVToNDC(uv01);

    switch (face) {
        case 0: return normalize(float3( 1.f,  -uv.y, -uv.x)); // +X
        case 1: return normalize(float3(-1.f,  -uv.y,  uv.x)); // -X
        case 2: return normalize(float3( uv.x,  1.f,  +uv.y)); // +Y
        case 3: return normalize(float3( uv.x, -1.f,  -uv.y)); // -Y
        case 4: return normalize(float3( uv.x, -uv.y,  1.f));  // +Z
        case 5: return normalize(float3(-uv.x, -uv.y, -1.f));  // -Z
    }

    return M3D_AXIS_X;
}


float3 PixelCoordToUVW(in uint3 coord, in uint3 size)
{
    size = max(size, ONEU3);

    return (float3(coord) + 0.5f) / float3(size);
}


float2 PixelCoordToUV(in uint2 coord, in uint2 size)
{
    return PixelCoordToUVW(uint3(coord, 0), uint3(size, 1)).xy;
}


float2 UVToNDC(in float2 uv)
{
    return 2.f * uv - 1.f;
}


float2 NDCToUV(in float2 ndc)
{
    return 0.5f * ndc + 0.5f;
}


float4 NDCToView(in float3 NDC, in float4x4 invProjMatr)
{
    float4 result = mul(invProjMatr, float4(NDC, 1.f));
    return result /= result.w;
}


float3 ViewPosFromDepth(in float2 uv, in float depth, in float4x4 invProjMatr)
{
    return NDCToView(float3(UVToNDC(uv), depth), invProjMatr).xyz;
}


float3 WPosFromDepth(in float2 uv, in float depth, in float4x4 invViewMatr, in float4x4 invProjMatr)
{
    const float3 viewPos = ViewPosFromDepth(uv, depth, invProjMatr);
    return mul(invViewMatr, float4(viewPos, 1.f)).xyz;
}


float3 WPosToNDC(in float3 wpos, in float4x4 viewProjMatr)
{
    const float4 clipPos = mul(viewProjMatr, float4(wpos, 1.f));
    return clipPos.xyz / clipPos.w;
}

#endif