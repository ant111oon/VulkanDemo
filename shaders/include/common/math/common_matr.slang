#ifndef MATH_COMMON_MATR_H
#define MATH_COMMON_MATR_H

#include "common.slang"


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


float4x4 LookAtRH(in float3 eye, in float3 target, in float3 up)
{
    const float3 f = normalize(target - eye);
    const float3 s = normalize(cross(f, up));
    const float3 u = cross(s, f);

    return float4x4(
        s.x,  s.y,  s.z, -dot(s, eye),
        u.x,  u.y,  u.z, -dot(u, eye),
       -f.x, -f.y, -f.z,  dot(f, eye),
        0.f,  0.f,  0.f,  1.f
    );

}


float4x4 OrthoRH_ZO(in float left, in float right, in float bottom, in float top, in float zNear, in float zFar)
{
#if defined(ENV_REVERSED_Z)
    const float n = zFar;
    const float f = zNear;
#else
    const float n = zNear;
    const float f = zFar;
#endif

#ifdef ENV_API_VULKAN
    static const float yFlip = -1.f;
#else
    static const float yFlip = 1.f;
#endif

    const float xScale = 2.f / (right - left);
    const float yScale = 2.f / (top - bottom) * yFlip;
    const float zScale = -rcp(f - n);

    const float xOffset = -(right + left) / (right - left);
    const float yOffset = -(top + bottom) / (top - bottom) * yFlip;
    const float zOffset = -n / (f - n);

    return float4x4(
        xScale, 0.f, 0.f, xOffset,
        0.f, yScale, 0.f, yOffset,
        0.f, 0.f, zScale, zOffset,
        0.f, 0.f, 0.f, 1.f
    );
}


float4x4 PerspectiveRH_ZO(in float fovY, in float aspect, in float zNear, in float zFar)
{
#if defined(ENV_REVERSED_Z)
    const float n = zFar;
    const float f = zNear;
#else
    const float n = zNear;
    const float f = zFar;
#endif

#ifdef ENV_API_VULKAN
    static const float yFlip = -1.f;
#else
    static const float yFlip = 1.f;
#endif

    const float tanHalfFovY = tan(fovY * 0.5f);

    const float xScale = rcp(aspect * tanHalfFovY);
    const float yScale = rcp(tanHalfFovY) * yFlip;
    const float zScale = -f / (f - n);

    return float4x4(
        xScale, 0.f, 0.f, 0.f,
        0.f, yScale, 0.f, 0.f,
        0.f, 0.f, zScale, -(f * n) / (f - n),
        0.f, 0.f,   -1.f, 0.f
    );
}

#endif