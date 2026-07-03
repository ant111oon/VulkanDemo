#include "pch.h"

#include "frustum.h"
#include "transform.h"


namespace math
{
    static glm::float3 GetFrustumCenterWCS(std::span<const glm::float3> corners)
    {
        glm::float3 center = ZEROF3;

        for (const glm::float3& corner : corners) {
            center += corner;
        }

        return center / static_cast<float>(corners.size());
    }


    static void GetFrustumPointsWCS_Inv(const glm::float4x4& invViewProj, std::span<glm::float3> outPoints, glm::float3& outCenter)
    {
    #ifdef ENG_GFX_API_VULKAN
        const float bottomY = 1.f;
        const float topY = -1.f;
    #else
        const float bottomY = -1.f;
        const float topY = 1.f;
    #endif

    #ifdef ENG_REVERSED_Z
        const float zNear = 1.f;
        const float zFar = 0.f;
    #else
        const float zNear = 0.f;
        const float zFar = 1.f;
    #endif

        outPoints[Frustum::POINT_NEAR_LEFT_BOTTOM]  = glm::float3(-1.f, bottomY, zNear);
        outPoints[Frustum::POINT_NEAR_RIGHT_BOTTOM] = glm::float3( 1.f, bottomY, zNear);
        outPoints[Frustum::POINT_NEAR_RIGHT_TOP]    = glm::float3( 1.f,    topY, zNear);
        outPoints[Frustum::POINT_NEAR_LEFT_TOP]     = glm::float3(-1.f,    topY, zNear);
        outPoints[Frustum::POINT_FAR_LEFT_BOTTOM]   = glm::float3(-1.f, bottomY, zFar);
        outPoints[Frustum::POINT_FAR_RIGHT_BOTTOM]  = glm::float3( 1.f, bottomY, zFar);
        outPoints[Frustum::POINT_FAR_RIGHT_TOP]     = glm::float3( 1.f,    topY, zFar);
        outPoints[Frustum::POINT_FAR_LEFT_TOP]      = glm::float3(-1.f,    topY, zFar);

        for (glm::float3& corner : outPoints) {
            glm::float4 temp = invViewProj * glm::float4(corner, 1.f);
            corner = glm::float3(temp / temp.w);
        }

        outCenter = GetFrustumCenterWCS(outPoints);
    }


    static void GetFrustumPlanes(std::span<const glm::float3> points, std::span<Plane> outPlanes)
    {
        const glm::float3& nlb = points[Frustum::POINT_NEAR_LEFT_BOTTOM];
        const glm::float3& nrb = points[Frustum::POINT_NEAR_RIGHT_BOTTOM];
        const glm::float3& nrt = points[Frustum::POINT_NEAR_RIGHT_TOP];
        const glm::float3& nlt = points[Frustum::POINT_NEAR_LEFT_TOP];
        const glm::float3& flb = points[Frustum::POINT_FAR_LEFT_BOTTOM];
        const glm::float3& frb = points[Frustum::POINT_FAR_RIGHT_BOTTOM];
        const glm::float3& frt = points[Frustum::POINT_FAR_RIGHT_TOP];
        const glm::float3& flt = points[Frustum::POINT_FAR_LEFT_TOP];

        outPlanes[Frustum::PLANE_LEFT].Construct(nlb, flb, flt);
        outPlanes[Frustum::PLANE_RIGHT].Construct(nrt, frt, nrb);
        
        outPlanes[Frustum::PLANE_BOTTOM].Construct(nlb, nrb, frb);
        outPlanes[Frustum::PLANE_TOP].Construct(nrt, nlt, frt);

        outPlanes[Frustum::PLANE_NEAR].Construct(nrb, nlb, nrt);
        outPlanes[Frustum::PLANE_FAR].Construct(flb, frb, frt);
    }


    Plane::Plane(const glm::vec3& norm, float dist)
        : normal(norm), distance(dist)
    {
        MATH_ASSERT(IsNormalized(normal));
    }


    void Plane::Construct(const glm::float3& A, const glm::float3& B, const glm::float3& C)
    {
        //      C
        //     / \
        //    /   \
        //   /     \
        //  /       \
        // A---------B

        const glm::float3 center = (A + B + C) / 3.f;

        normal = glm::normalize(glm::cross(B - A, C - A));
        distance = -glm::dot(normal, center);
    }


    float Plane::DistanceTo(const glm::float3& pt) const
    {
        return glm::dot(normal, pt) + distance;
    }


    Frustum::Frustum(const glm::float3& position, const glm::quat& rotation, const glm::float4x4& proj)
    {
        Construct(position, rotation, proj);
    }


    Frustum::Frustum(const glm::float4x4& view, const glm::float4x4& proj)
    {
        Construct(view, proj);
    }


    Frustum::Frustum(const glm::float4x4& viewProj)
    {
        Construct(viewProj);
    }


    Frustum::Frustum(const glm::float3& position, const glm::quat& rotation, float fovY, float aspectRatio, float zNear, float zFar)
    {
        Construct(position, rotation, fovY, aspectRatio, zNear, zFar);
    }


    Frustum::Frustum(const glm::float3& position, const glm::quat& rotation, float left, float right, float bottom, float top, float zNear, float zFar)
    {
        Construct(position, rotation, left, right, bottom, top, zNear, zFar);
    }
    

    void Frustum::Construct(const glm::float3& position, const glm::quat& rotation, const glm::float4x4& proj)
    {
        const glm::float4x4 rotMat = glm::mat4_cast(glm::inverse(rotation));
        const glm::float4x4 transMat = glm::translate(M3D_MAT4X4_IDENTITY, -position);

        const glm::float4x4 view = rotMat * transMat;

        Construct(view, proj);
    }


    void Frustum::Construct(const glm::float4x4& view, const glm::float4x4& proj)
    {
        Construct(proj * view);
    }


    void Frustum::Construct(const glm::float4x4& viewProj)
    {
        const glm::float4x4 invViewProj = glm::inverse(viewProj);

        GetFrustumPointsWCS_Inv(invViewProj, m_points, m_center);
        GetFrustumPlanes(m_points, m_planes);
    }


    void Frustum::Construct(const glm::float3& position, const glm::quat& rotation, float fovY, float aspectRatio, float zNear, float zFar)
    {
        MATH_ASSERT(IsNormalized(rotation));
        MATH_ASSERT(fovY > 0.f);
        MATH_ASSERT(aspectRatio > 0.f);
        MATH_ASSERT(zNear > 0.f);
        MATH_ASSERT(zNear < zFar);

    #if defined(ENG_REVERSED_Z)
        std::swap(zNear, zFar);
    #endif

        glm::float4x4 proj = glm::perspective(fovY, aspectRatio, zNear, zFar);

    #ifdef ENG_GFX_API_VULKAN
        proj[1][1] *= -1.f;
    #endif

        Construct(position, rotation, proj);
    }


    void Frustum::Construct(const glm::float3& position, const glm::quat& rotation, float left, float right, float bottom, float top, float zNear, float zFar)
    {
        MATH_ASSERT(IsNormalized(rotation));
        MATH_ASSERT(zNear > 0.f);
        MATH_ASSERT(zNear < zFar);
        MATH_ASSERT(left < right);
        MATH_ASSERT(bottom < top);

    #if defined(ENG_REVERSED_Z)
        std::swap(zNear, zFar);
    #endif

        glm::float4x4 proj = glm::ortho(left, right, bottom, top, zNear, zFar);

    #ifdef ENG_GFX_API_VULKAN
        proj[1][1] *= -1.f;
    #endif

        Construct(position, rotation, proj);
    }


    bool Frustum::IsIntersect(const AABB& aabb) const
    {
        for (size_t i = 0; i < 6; ++i) {
            const Plane& plane = m_planes[i];

            glm::float3 p = glm::float3(
                (plane.normal.x >= 0.f) ? aabb.max.x : aabb.min.x,
                (plane.normal.y >= 0.f) ? aabb.max.y : aabb.min.y,
                (plane.normal.z >= 0.f) ? aabb.max.z : aabb.min.z
            );

            if (glm::dot(plane.normal, p) + plane.distance < 0.f) {
                return false;
            }
        }

        return true;
    }


    const Plane& Frustum::GetPlane(uint32_t index) const
    {
        MATH_ASSERT(index < PLANE_COUNT);
        return m_planes[index];
    }


    const glm::float3& Frustum::GetPoint(uint32_t index) const
    {
        MATH_ASSERT(index < POINT_COUNT);
        return m_points[index];
    }
}