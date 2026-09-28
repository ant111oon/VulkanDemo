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
    float3 f = normalize(target - eye);
    float3 s = normalize(cross(f, up));
    float3 u = cross(s, f);

    float4x4 result = M3D_IDENTITY_4X4;

    result[0][0] = s.x;
    result[1][0] = s.y;
    result[2][0] = s.z;
    result[0][1] = u.x;
    result[1][1] = u.y;
    result[2][1] = u.z;
    result[0][2] =-f.x;
    result[1][2] =-f.y;
    result[2][2] =-f.z;
    result[3][0] =-dot(s, eye);
    result[3][1] =-dot(u, eye);
    result[3][2] = dot(f, eye);
    
    return result;
}


float4x4 OrthoRH_ZO(in float left, in float right, in float bottom, in float top, in float zNear, in float zFar)
{
    float4x4 result = M3D_IDENTITY_4X4;

    result[0][0] = 2.f / (right - left);
    result[1][1] = 2.f / (top - bottom);
    result[2][2] = -rcp(zFar - zNear);
    result[3][0] = -(right + left) / (right - left);
    result[3][1] = -(top + bottom) / (top - bottom);
    result[3][2] = -zNear / (zFar - zNear);

    return result;
}


float4x4 PerspectiveRH_ZO(in float fovy, in float aspect, in float zNear, in float zFar)
{
    const float tanHalfFovy = tan(fovy * 0.5f);

    float4x4 result = float4x4(0.f);

    result[0][0] = rcp(aspect * tanHalfFovy);
    result[1][1] = rcp(tanHalfFovy);
    result[2][2] = zFar / (zNear - zFar);
    result[2][3] = -1.f;
    result[3][2] = -(zFar * zNear) / (zFar - zNear);

    return result;
}

#endif