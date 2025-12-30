#ifndef MATH_H
#define MATH_H


static const float M3D_EPS     = 0.000001f;
static const float M3D_PI      = 3.14159265359f;
static const float M3D_2_PI    = 2.0f * M3D_PI;
static const float M3D_HALF_PI = 0.5f * M3D_PI;

static const float M3D_FLT_MAX = 3.40282347e+37F;
static const float M3D_FLT_MIN = 1.17549435e-37F;

#define FLOAT2(x) float2(x, x)
#define FLOAT3(x) float3(x, x, x)
#define FLOAT4(x) float4(x, x, x, x)

static const float2 M3D_ZEROF2 = FLOAT2(0.f);
static const float3 M3D_ZEROF3 = FLOAT3(0.f);
static const float4 M3D_ZEROF4 = FLOAT4(0.f);
static const float2 M3D_ONEF2  = FLOAT2(1.f);
static const float3 M3D_ONEF3  = FLOAT3(1.f);
static const float4 M3D_ONEF4  = FLOAT4(1.f);

static const float3 M3D_AXIS_X = float3(1.f, 0.f, 0.f);
static const float3 M3D_AXIS_Y = float3(0.f, 1.f, 0.f);
static const float3 M3D_AXIS_Z = float3(0.f, 0.f, 1.f);

static const float3x3 M3D_IDENTITY_3X3 = float3x3(
    1.f, 0.f, 0.f,
    0.f, 1.f, 0.f,
    0.f, 0.f, 1.f
);

static const float4x4 M3D_IDENTITY_4X4 = float4x4(
    1.f, 0.f, 0.f, 0.f,
    0.f, 1.f, 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f,
    0.f, 0.f, 0.f, 1.f
);

static const float3x4 M3D_IDENTITY_3X4 = float3x4(
    1.f, 0.f, 0.f, 0.f,
    0.f, 1.f, 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f,
);

static const float4x3 M3D_IDENTITY_4X3 = float4x3(
    1.f, 0.f, 0.f,
    0.f, 1.f, 0.f,
    0.f, 0.f, 1.f,
    0.f, 0.f, 0.f
);


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


bool IsFlagSet(uint value, uint mask)
{
    return (value & mask) != 0;
}


bool IsBitSet(uint value, uint bit)
{
    return IsFlagSet(value, uint(1) << bit);
}

#define M3D_IS_ZERO(x)      IsZero(x)
#define M3D_IS_EQUAL(l, r)  M3D_IS_ZERO((l) - (r))


float3x3 CalcTBN(in float3 tang, in float3 norm)
{
    const float3 N = normalize(norm);
    const float3 T = normalize(tang - dot(tang, N) * N);

    const float3 B = normalize(cross(N, T));

    return float3x3(T, B, N);
}


#endif