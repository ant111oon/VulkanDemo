#ifndef MATH_GEOM_PRIMITIVES_H
#define MATH_GEOM_PRIMITIVES_H

#include "common/math/constants.slang"
#include "common/math/common_matr.slang"


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
        const float3 center = (A + B + C) / 3.f;

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
        minUV = ZEROF2;
        maxUV = ONEF2;

    #ifdef ENV_REVERSED_Z
        nearZ = 1.f;
        farZ = 0.f;
    #else
        nearZ = 0.f;
        farZ = 1.f;
    #endif
    }

    __init(in float2 minUV, in float2 maxUV, in float nearZ, in float farZ)
    {
        this.minUV = minUV;
        this.maxUV = maxUV;
        this.nearZ = nearZ;
        this.farZ = farZ;
    }

    [mutating]
    void BeginUpdate()
    {
        minUV = ONEF2;
        maxUV = ZEROF2;

    #ifdef ENV_REVERSED_Z
        nearZ = 0.f;
        farZ = 1.f;
    #else
        nearZ = 1.f;
        farZ = 0.f;
    #endif
    }

    [mutating]
    void UpdateUV(in float2 uv)
    {
        minUV = min(minUV, uv);
        maxUV = max(maxUV, uv);
    }

    [mutating]
    void UpdateZ(in float z)
    {
    #ifdef ENV_REVERSED_Z
        nearZ = max(nearZ, z);
        farZ = min(farZ, z);
    #else
        nearZ = min(nearZ, z);
        farZ = max(farZ, z);
    #endif
    }

    [mutating]
    void UpdateUVZ(in float3 uvz)
    {
        UpdateUV(uvz.xy);
        UpdateZ(uvz.z);
    }

    [mutating]
    void Saturate()
    {
        minUV = saturate(minUV);
        maxUV = saturate(maxUV);

        nearZ = saturate(nearZ);
        farZ = saturate(farZ);
    }

    property float minU { get { return minUV.x; } }
    property float minV { get { return minUV.y; } }

    property float maxU { get { return maxUV.x; } }
    property float maxV { get { return maxUV.y; } }

    property float deltaU { get { return maxU - minU; } }
    property float deltaV { get { return maxV - minV; } }
    property float deltaZ { get { return abs(farZ - nearZ); } }

    property float2 deltaUV { get { return maxUV - minUV; } }

    property float2 cornerLB { get { return minUV; } }
    property float2 cornerRB { get { return float2(maxU, minV); } }
    property float2 cornerLU { get { return float2(minU, maxV); } }
    property float2 cornerRU { get { return maxUV; } }

    float2 minUV;
    float  nearZ;
    float2 maxUV;
    float  farZ;
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

    void GetVertices(out float3 verts[8])
    {
        const float3 s = size;

        verts[0] = minn;
        verts[1] = minn + float3(s.x, 0.f, 0.f);
        verts[2] = minn + float3(0.f, s.y, 0.f);
        verts[3] = minn + float3(0.f, 0.f, s.z);
        verts[4] = minn + float3(s.xy, 0.f);
        verts[5] = minn + float3(0.f, s.yz);
        verts[6] = minn + float3(s.x, 0.f, s.z);
        verts[7] = minn + s;
    }

    float DistanceTo(in float3 pt)
    {
        const float3 q = abs(pt - center) - halfSize;

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

            sbox.UpdateUV(UV);
            sbox.UpdateZ(NDC.z);
        }

        return sbox;
    }

    bool IsInside(in float3 pt)
    {
        return all(pt >= minn) && all(pt <= maxx);
    }

    bool IntersectsPlane(in GPU_Plane plane)
    {
        const float projectedRadius = dot(abs(plane.normal), halfSize);
        const float signedDistance = plane.DistanceTo(center);

        return abs(signedDistance) <= projectedRadius;
    }

    property float3 center
    {
        get { return (minn + maxx) * 0.5f; }
    }

    property float3 size
    {
        get { return maxx - minn; }
    }

    property float3 halfSize
    {
        get { return size * 0.5f; }
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

            if (plane.DistanceTo(sphere.position) < -sphere.radius) {
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

            if (plane.DistanceTo(p) < 0.f) {
                return false;
            }
        }

        return true;
    }

    GPU_Plane GetPlane(PlaneID ID) { return planes[ID]; }

    property GPU_Plane leftPlane { get { return planes[PlaneID::PLANE_LEFT]; } }
    property GPU_Plane topPlane { get { return planes[PlaneID::PLANE_TOP]; } }
    property GPU_Plane rightPlane { get { return planes[PlaneID::PLANE_RIGHT]; } }
    property GPU_Plane bottomPlane { get { return planes[PlaneID::PLANE_BOTTOM]; } }
    property GPU_Plane nearPlane { get { return planes[PlaneID::PLANE_NEAR]; } }
    property GPU_Plane farPlane { get { return planes[PlaneID::PLANE_FAR]; } }

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