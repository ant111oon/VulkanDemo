#ifndef MATH_GEOM_PRIMITIVES_H
#define MATH_GEOM_PRIMITIVES_H

#include "common/math/constants.slang"
#include "common/math/common.slang"


struct GPU_Plane
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

    __init(in float3 A, in float3 B, in float3 C)
    {
        Construct(A, B, C);
    }

    [mutating]
    void Construct(in float3 A, in float3 B, in float3 C)
    {
        const float3 center = (A + B + C) * rcp(3.f);

        this.normal = normalize(cross(B - A, C - A));
        this.distance = -dot(normal, center);
    }

    float DistanceTo(in float3 pt)
    {
        return dot(normal, pt) + distance;
    }

    float3 normal;
    float distance;
};


struct GPU_Sphere
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


struct GPU_SBox
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


struct GPU_AABB
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

    float3 ClosestPoint(in float3 pt)
    {
        return clamp(pt, minn, maxx);
    }

    GPU_SBox GetSBox(in float4x4 viewProjMatr)
    {
        float3 verts[8];
        GetVertices(verts);

        GPU_SBox sbox = GPU_SBox();
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

    bool IntersectsPlane(in GPU_Plane plane)
    {
        const float3 center = GetCenter();
        const float3 size = GetSize();

        const float projectedRadius = dot(abs(plane.normal), size);
        const float signedDistance = dot(plane.normal, center) + plane.distance;

        return abs(signedDistance) <= projectedRadius;
    }

    float3 minn;
    float3 maxx;
};


struct GPU_Frustum
{
    [mutating]
    void Construct(in float4x4 view, in float4x4 proj)
    {
        Construct(proj * view);
    }

    [mutating]
    void Construct(in float4x4 viewProj)
    {
        const float4x4 invViewProj = Inverse(viewProj);

        float3 points[PointID::POINT_COUNT];

        GetFrustumPointsWCS_Inv(invViewProj, points);
        GetFrustumPlanes(points, planes);
    }

    bool IsIntersect(in GPU_Sphere sphere)
    {
        for (uint i = 0; i < 6; ++i) {
            GPU_Plane plane = planes[i];

            if (dot(plane.normal, sphere.position) + plane.distance < -sphere.radius) {
                return false;
            }
        }

        return true;
    }

    bool IsIntersect(in GPU_AABB aabb)
    {
        for (uint i = 0; i < 6; ++i) {
            GPU_Plane plane = planes[i];

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

    GPU_Plane GetPlane(PlaneID ID) { return planes[ID]; }
    GPU_Plane GetLeftPlane()       { return planes[PlaneID::PLANE_LEFT]; }
    GPU_Plane GetTopPlane()        { return planes[PlaneID::PLANE_TOP]; }
    GPU_Plane GetRightPlane()      { return planes[PlaneID::PLANE_RIGHT]; }
    GPU_Plane GetBottomPlane()     { return planes[PlaneID::PLANE_BOTTOM]; }
    GPU_Plane GetNearPlane()       { return planes[PlaneID::PLANE_NEAR]; }
    GPU_Plane GetFarPlane()        { return planes[PlaneID::PLANE_FAR]; }

    // Must be sync with Frustum::PlaneID in frustum.h
    enum PlaneID
    {
        PLANE_LEFT,
        PLANE_TOP,
        PLANE_RIGHT,
        PLANE_BOTTOM,
        PLANE_NEAR,
        PLANE_FAR,

        PLANE_COUNT
    };

    // Must be sync with Frustum::PointID in frustum.h
    enum PointID
    {
        NEAR_LEFT_BOTTOM,
        NEAR_RIGHT_BOTTOM,
        NEAR_RIGHT_TOP,
        NEAR_LEFT_TOP,

        FAR_LEFT_BOTTOM,
        FAR_RIGHT_BOTTOM,
        FAR_RIGHT_TOP,
        FAR_LEFT_TOP,

        POINT_COUNT
    };

    private static void GetFrustumPointsWCS_Inv(in float4x4 invViewProj, inout float3 outPoints[PointID::POINT_COUNT])
    {
    #ifdef ENV_API_VULKAN
        static const float bottomY = 1.f;
        static const float topY = -1.f;
    #else
        static const float bottomY = -1.f;
        static const float topY = 1.f;
    #endif

    #ifdef ENV_REVERSED_Z
        static const float zNear = 1.f;
        static const float zFar = 0.f;
    #else
        static const float zNear = 0.f;
        static const float zFar = 1.f;
    #endif

        outPoints[PointID::NEAR_LEFT_BOTTOM]  = float3(-1.f, bottomY, zNear);
        outPoints[PointID::NEAR_RIGHT_BOTTOM] = float3( 1.f, bottomY, zNear);
        outPoints[PointID::NEAR_RIGHT_TOP]    = float3( 1.f,    topY, zNear);
        outPoints[PointID::NEAR_LEFT_TOP]     = float3(-1.f,    topY, zNear);
        outPoints[PointID::FAR_LEFT_BOTTOM]   = float3(-1.f, bottomY, zFar);
        outPoints[PointID::FAR_RIGHT_BOTTOM]  = float3( 1.f, bottomY, zFar);
        outPoints[PointID::FAR_RIGHT_TOP]     = float3( 1.f,    topY, zFar);
        outPoints[PointID::FAR_LEFT_TOP]      = float3(-1.f,    topY, zFar);

        for (uint i = 0; i < PointID::POINT_COUNT; ++i) {
            const float3 pt = outPoints[i];
            const float4 temp = mul(invViewProj, float4(pt, 1.f));
            
            outPoints[i] = float3(temp.xyz / temp.w);
        }
    }

    private static void GetFrustumPlanes(in float3 points[PointID::POINT_COUNT], out GPU_Plane outPlanes[PlaneID::PLANE_COUNT])
    {
        outPlanes[PlaneID::PLANE_LEFT]
            .Construct(
                points[PointID::NEAR_LEFT_BOTTOM],
                points[PointID::FAR_LEFT_BOTTOM],
                points[PointID::FAR_LEFT_TOP]
            );

        outPlanes[PlaneID::PLANE_RIGHT]
            .Construct(
                points[PointID::NEAR_RIGHT_TOP],
                points[PointID::FAR_RIGHT_TOP],
                points[PointID::NEAR_RIGHT_BOTTOM]
            );

        outPlanes[PlaneID::PLANE_BOTTOM]
            .Construct(
                points[PointID::NEAR_LEFT_BOTTOM],
                points[PointID::NEAR_RIGHT_BOTTOM],
                points[PointID::FAR_RIGHT_BOTTOM]
            );

        outPlanes[PlaneID::PLANE_TOP]
            .Construct(
                points[PointID::NEAR_RIGHT_TOP],
                points[PointID::NEAR_LEFT_TOP],
                points[PointID::FAR_RIGHT_TOP]
            );

        outPlanes[PlaneID::PLANE_NEAR]
            .Construct(
                points[PointID::NEAR_RIGHT_BOTTOM],
                points[PointID::NEAR_LEFT_BOTTOM],
                points[PointID::NEAR_RIGHT_TOP]
            );

        outPlanes[PlaneID::PLANE_FAR]
            .Construct(
                points[PointID::FAR_LEFT_BOTTOM],
                points[PointID::FAR_RIGHT_BOTTOM],
                points[PointID::FAR_RIGHT_TOP]
            );
    }

    private GPU_Plane planes[6];
};


#endif