#ifndef MATH_GEOM_PRIMITIVES_H
#define MATH_GEOM_PRIMITIVES_H

#include "common/math/constants.slang"
#include "common/math/common.slang"


struct SBOX
{
    __init()
    {
        this.minUV = ZEROF2;
        this.maxUV = ONEF2;

    #ifdef ENV_REVERSED_Z
        this.closestZ = 1.f;
    #else
        this.closestZ = 0.f;
    #endif
    }

    __init(in float2 minUV, in float2 maxUV, in float closestZ)
    {
        this.minUV = minUV;
        this.maxUV = maxUV;
        this.closestZ = closestZ;
    }

    [mutating]
    void BeginUpdate()
    {
        this.minUV = ONEF2;
        this.maxUV = ZEROF2;

    #ifdef ENV_REVERSED_Z
        this.closestZ = 0.f;
    #else
        this.closestZ = 1.f;
    #endif
    }

    [mutating]
    void UpdateMinUV(in float2 newMinUV)
    {
        minUV = min(minUV, newMinUV);
    }

    [mutating]
    void UpdateMaxUV(in float2 newMaxUV)
    {
        maxUV = max(maxUV, newMaxUV);
    }

    [mutating]
    void UpdateClosestZ(in float newClosestZ)
    {
    #ifdef ENV_REVERSED_Z
        closestZ = max(closestZ, newClosestZ);
    #else
        closestZ = min(closestZ, newClosestZ);
    #endif
    }

    [mutating]
    void Saturate()
    {
        this.minUV = saturate(this.minUV);
        this.maxUV = saturate(this.maxUV);
        this.closestZ = saturate(this.closestZ);
    }

    float2 GetDeltaUV() { return maxUV - minUV; }

    float2 GetCornerBL() { return minUV; }
    float2 GetCornerBR() { return float2(maxUV.x, minUV.y); }
    float2 GetCornerUL() { return float2(minUV.x, maxUV.y); }
    float2 GetCornerUR() { return maxUV; }

    float2 minUV;
    float2 maxUV;
    float closestZ;
};


struct SPHERE
{
    __init()
    {
        this.position = ZEROF3;
        this.radius = M3D_EPS;
    }

    __init(in float3 position, in float radius)
    {
        this.position = position;
        this.radius = radius;
    }

    float DistanceTo(in float3 pt)
    {
        return distance(position, pt) - radius;
    }

    float3 position;
    float radius;
};


struct AABB
{
    __init()
    {
        this.minn = -EPSF3;
        this.maxx = EPSF3;
    }

    __init(in float3 minn, in float3 maxx)
    {
        this.minn = minn;
        this.maxx = maxx;
    }

    float3 GetCenter()
    {
        return (minn + maxx) * 0.5f;
    }

    float3 GetSize()
    {
        return maxx - minn;
    }

    float3 GetHalfSize()
    {
        return GetSize() * 0.5f;
    }

    void GetVertices(out float3 verts[8])
    {
        const float3 size = GetSize();

        verts[0] = minn;
        verts[1] = minn + float3(size.x, 0.f, 0.f);
        verts[2] = minn + float3(0.f, size.y, 0.f);
        verts[3] = minn + float3(0.f, 0.f, size.z);
        verts[4] = minn + float3(size.xy, 0.f);
        verts[5] = minn + float3(0.f, size.yz);
        verts[6] = minn + float3(size.x, 0.f, size.z);
        verts[7] = minn + size;
    }

    float DistanceTo(in float3 pt)
    {
        const float3 q = abs(pt - GetCenter()) - GetHalfSize();

        return length(max(q, 0.f)) + min(max(q.x, max(q.y, q.z)), 0.f);
    }

    SBOX GetSBOX(in float4x4 viewProjMatr)
    {
        float3 verts[8];
        GetVertices(verts);

        SBOX sbox = SBOX();
        sbox.BeginUpdate();

        for (uint i = 0; i < 8; ++i) {
            const float3 NDC = WPosToNDC(verts[i], viewProjMatr);
            const float2 UV = NDCToUV(NDC.xy);

            sbox.UpdateMinUV(UV);
            sbox.UpdateMaxUV(UV);
            sbox.UpdateClosestZ(NDC.z);
        }

        return sbox;
    }

    bool IsInside(in float3 pt)
    {
        return all(pt >= minn) && all(pt <= maxx);
    }

    float3 minn;
    float3 maxx;
};


struct PLANE
{
    __init()
    {
        this.normal = M3D_AXIS_X;
        this.distance = 0.f;
    }

    __init(in float3 normal, in float distance)
    {
        this.normal = normal;
        this.distance = distance;
    }

    float3 normal;
    float distance;
};


struct FRUSTUM
{
    bool IsIntersect(in SPHERE sphere)
    {
        for (uint i = 0; i < 6; ++i) {
            PLANE plane = planes[i];

            if (dot(plane.normal, sphere.position) + plane.distance < -sphere.radius) {
                return false;
            }
        }

        return true;
    }

    bool IsIntersect(in AABB aabb)
    {
        for (uint i = 0; i < 6; ++i) {
            PLANE plane = planes[i];

            const float3 p = float3(
                (plane.normal.x >= 0.f) ? aabb.maxx.x : aabb.minn.x,
                (plane.normal.y >= 0.f) ? aabb.maxx.y : aabb.minn.y,
                (plane.normal.z >= 0.f) ? aabb.maxx.z : aabb.minn.z
            );

            if (dot(plane.normal, p) + plane.distance < 0.f) {
                return false;
            }
        }

        return true;
    }

    PLANE planes[6];
};


#endif