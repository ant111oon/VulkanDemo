#ifndef MATH_COMMON_H
#define MATH_COMMON_H

#include "constants.slang"


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


void Swap<T>(inout T left, inout T right)
{
    const T tmp = left;
    left = right;
    right = tmp;
}


void CalcTangBitang(in float3 norm, out float3 tang, out float3 bitang)
{
    const float3 up = abs(norm.z) < 0.999f ? M3D_AXIS_Z : M3D_AXIS_X;
    
    tang = normalize(cross(up, norm));
    bitang = cross(norm, tang);
}


float3x3 CalcTBN(in float4 tangent, in float3 normal)
{
    float3 T = normalize(tangent.xyz);
    T = normalize(T - normal * dot(normal, T));

    const float3 B = cross(normal, T) * tangent.w;

    return float3x3(T, B, normal);
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


float OrthoDepthToViewDepth(in float depth, in float near, in float far)
{
    float depth01 = saturate(depth);

#ifdef ENV_REVERSED_Z
    depth01 = 1.f - depth01;
#endif

    return lerp(near, far, depth01);
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


uint CeilDiv(in uint value, in uint divisor)
{
    return (value + divisor - 1) / divisor;
}


float4x4 Inverse(in float4x4 m)
{
    float coef00 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
    float coef02 = m[1][2] * m[3][3] - m[3][2] * m[1][3];
    float coef03 = m[1][2] * m[2][3] - m[2][2] * m[1][3];

    float coef04 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
    float coef06 = m[1][1] * m[3][3] - m[3][1] * m[1][3];
    float coef07 = m[1][1] * m[2][3] - m[2][1] * m[1][3];

    float coef08 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
    float coef10 = m[1][1] * m[3][2] - m[3][1] * m[1][2];
    float coef11 = m[1][1] * m[2][2] - m[2][1] * m[1][2];

    float coef12 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
    float coef14 = m[1][0] * m[3][3] - m[3][0] * m[1][3];
    float coef15 = m[1][0] * m[2][3] - m[2][0] * m[1][3];

    float coef16 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
    float coef18 = m[1][0] * m[3][2] - m[3][0] * m[1][2];
    float coef19 = m[1][0] * m[2][2] - m[2][0] * m[1][2];

    float coef20 = m[2][0] * m[3][1] - m[3][0] * m[2][1];
    float coef22 = m[1][0] * m[3][1] - m[3][0] * m[1][1];
    float coef23 = m[1][0] * m[2][1] - m[2][0] * m[1][1];

    float4 fac0 = float4(coef00, coef00, coef02, coef03);
    float4 fac1 = float4(coef04, coef04, coef06, coef07);
    float4 fac2 = float4(coef08, coef08, coef10, coef11);
    float4 fac3 = float4(coef12, coef12, coef14, coef15);
    float4 fac4 = float4(coef16, coef16, coef18, coef19);
    float4 fac5 = float4(coef20, coef20, coef22, coef23);

    float4 vec0 = float4(m[1][0], m[0][0], m[0][0], m[0][0]);
    float4 vec1 = float4(m[1][1], m[0][1], m[0][1], m[0][1]);
    float4 vec2 = float4(m[1][2], m[0][2], m[0][2], m[0][2]);
    float4 vec3 = float4(m[1][3], m[0][3], m[0][3], m[0][3]);

    float4 inv0 = float4(vec1 * fac0 - vec2 * fac1 + vec3 * fac2);
    float4 inv1 = float4(vec0 * fac0 - vec2 * fac3 + vec3 * fac4);
    float4 inv2 = float4(vec0 * fac1 - vec1 * fac3 + vec3 * fac5);
    float4 inv3 = float4(vec0 * fac2 - vec1 * fac4 + vec2 * fac5);

    float4 signA = float4(+1, -1, +1, -1);
    float4 signB = float4(-1, +1, -1, +1);
    float4x4 inv = float4x4(inv0 * signA, inv1 * signB, inv2 * signA, inv3 * signB);

    float4 row0 = float4(inv[0][0], inv[1][0], inv[2][0], inv[3][0]);

    float4 dot0 = float4(m[0] * row0);
    float dot1 = (dot0.x + dot0.y) + (dot0.z + dot0.w);

    return inv * rcp(dot1);
}

#endif