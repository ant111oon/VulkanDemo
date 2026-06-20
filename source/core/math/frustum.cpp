#include "pch.h"

#include "frustum.h"
#include "transform.h"


namespace math
{
    Plane::Plane(const glm::vec3& norm, float dist)
        : normal(norm), distance(dist)
    {
        MATH_ASSERT(IsNormalized(normal));
    }


    float Plane::DistanceTo(const glm::float3& pt) const
    {
        return glm::dot(normal, pt) + distance;
    }


    Frustum::Frustum(const glm::float3& position, const glm::float3& rightDir, const glm::float3& upDir, float fovY, float aspectRatio, float zNear, float zFar)
    {
        Construct(position, rightDir, upDir, fovY, aspectRatio, zNear, zFar);
    }


    Frustum::Frustum(const glm::float3& position, const glm::quat& rotation, float fovY, float aspectRatio, float zNear, float zFar)
    {
        Construct(position, rotation, fovY, aspectRatio, zNear, zFar);
    }


    Frustum::Frustum(const glm::float4x4& transform, float fovY, float aspectRatio, float zNear, float zFar)
    {
        Construct(transform, fovY, aspectRatio, zNear, zFar);
    }


    void Frustum::Construct(const glm::float3& position, const glm::quat& rotation, float fovY, float aspectRatio, float zNear, float zFar)
    {
        Construct(position, rotation * M3D_AXIS_X, rotation * M3D_AXIS_Y, fovY, aspectRatio, zNear, zFar);
    }


    void Frustum::Construct(const glm::float4x4& transform, float fovY, float aspectRatio, float zNear, float zFar)
    {
        glm::float3 position;
        glm::quat rotation;
        glm::float3 scale;

        math::GetTRSComponents(transform, position, rotation, scale);

        Construct(position, rotation, fovY, aspectRatio, zNear, zFar);
    }


    void Frustum::Construct(const glm::float3& position, const glm::float3& rightDir, const glm::float3& upDir, float fovY, float aspectRatio, float zNear, float zFar)
    {
        MATH_ASSERT(IsNormalized(rightDir));
        MATH_ASSERT(IsNormalized(upDir));
        MATH_ASSERT(fovY > 0.f);
        MATH_ASSERT(aspectRatio > 0.f);
        MATH_ASSERT(zNear > 0.f);
        MATH_ASSERT(zNear < zFar);

        using namespace math;

        const glm::float3 backwardDir = glm::cross(rightDir, upDir);
        const glm::float3 forwardDir = -backwardDir;
        const glm::float3 farVec = forwardDir * zFar;
        const float halfH = zFar * glm::tan(fovY * 0.5f);
        const float halfW = halfH * aspectRatio;

        Plane& leftPlane = planes[FR_PLANE_LEFT];
        leftPlane.normal = glm::normalize(glm::cross(glm::normalize(farVec - rightDir * halfW), upDir));
        leftPlane.distance = -glm::dot(leftPlane.normal, position);
        
        Plane& topPlane = planes[FR_PLANE_TOP];
        topPlane.normal = glm::normalize(glm::cross(glm::normalize(farVec + upDir * halfH), rightDir));
        topPlane.distance = -glm::dot(topPlane.normal, position);

        Plane& rightPlane = planes[FR_PLANE_RIGHT];
        rightPlane.normal = glm::normalize(glm::cross(glm::normalize(farVec + rightDir * halfW), -upDir));
        rightPlane.distance = -glm::dot(rightPlane.normal, position);

        Plane& bottomPlane = planes[FR_PLANE_BOTTOM];
        bottomPlane.normal = glm::normalize(glm::cross(glm::normalize(farVec - upDir * halfH), -rightDir));
        bottomPlane.distance = -glm::dot(bottomPlane.normal, position);

        Plane& nearPlane = planes[FR_PLANE_NEAR];
        nearPlane.normal = forwardDir;
        nearPlane.distance = -glm::dot(nearPlane.normal, position + forwardDir * zNear);

        Plane& farPlane = planes[FR_PLANE_FAR];
        farPlane.normal = backwardDir;
        farPlane.distance = -glm::dot(farPlane.normal, position + forwardDir * zFar);
    }


    bool Frustum::IsIntersect(const AABB& aabb) const
    {
        for (size_t i = 0; i < 6; ++i) {
            const Plane& plane = planes[i];

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


    const Plane& Frustum::GetPlane(FrustumPlaneIndex index) const
    {
        MATH_ASSERT(index < FR_PLANE_COUNT);
        return planes[index];
    }


    FrustumCorners GetFrustumCornersWCS(const glm::float4x4& view, const glm::float4x4& proj)
    {
        return GetFrustumCornersWCS(proj * view);
    }


    FrustumCorners GetFrustumCornersWCS(const glm::float4x4& viewProj)
    {
        const glm::float4x4 invViewProj = glm::inverse(viewProj);

        return GetFrustumCornersWCS_Inv(invViewProj);
    }


    FrustumCorners GetFrustumCornersWCS_Inv(const glm::float4x4& invViewProj)
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

        FrustumCorners corners = {};
        corners[FR_CORNER_LEFT_BOTTOM_NEAR]  = glm::float3(-1.f, bottomY, zNear);
        corners[FR_CORNER_RIGHT_BOTTOM_NEAR] = glm::float3( 1.f, bottomY, zNear);
        corners[FR_CORNER_RIGHT_TOP_NEAR]    = glm::float3( 1.f,    topY, zNear);
        corners[FR_CORNER_LEFT_TOP_NEAR]     = glm::float3(-1.f,    topY, zNear);
        corners[FR_CORNER_LEFT_BOTTOM_FAR]   = glm::float3(-1.f, bottomY, zFar);
        corners[FR_CORNER_RIGHT_BOTTOM_FAR]  = glm::float3( 1.f, bottomY, zFar);
        corners[FR_CORNER_RIGHT_TOP_FAR]     = glm::float3( 1.f,    topY, zFar);
        corners[FR_CORNER_LEFT_TOP_FAR]      = glm::float3(-1.f,    topY, zFar);

        for (glm::float3& corner : corners) {
            glm::float4 temp = invViewProj * glm::float4(corner, 1.f);
            corner = glm::float3(temp / temp.w);
        }

        return corners;
    }


    glm::float3 GetFrustumCenterWCS(const FrustumCorners& corners)
    {
        glm::float3 center = ZEROF3;

        for (const glm::float3& corner : corners) {
            center += corner;
        }

        return center / static_cast<float>(corners.size());
    }
}