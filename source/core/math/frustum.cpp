#include "pch.h"

#include "frustum.h"


namespace math
{
    Plane::Plane(const glm::vec3& norm, float dist)
        : normal(norm), distance(dist)
    {
        MATH_ASSERT(IsNormalized(normal));
    }


    Frustum::Frustum(const glm::float3& position, const glm::float3& rightDir, const glm::float3& upDir, float fovY, float aspectRatio, float zNear, float zFar)
    {
        Recalculate(position, rightDir, upDir, fovY, aspectRatio, zNear, zFar);
    }


    void Frustum::Recalculate(const glm::float3& position, const glm::float3& rightDir, const glm::float3& upDir, float fovY, float aspectRatio, float zNear, float zFar)
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

        Plane& leftPlane = planes[Frustum::PLANE_IDX_LEFT];
        leftPlane.normal = glm::normalize(glm::cross(glm::normalize(farVec - rightDir * halfW), upDir));
        leftPlane.distance = -glm::dot(leftPlane.normal, position);
        
        Plane& topPlane = planes[Frustum::PLANE_IDX_TOP];
        topPlane.normal = glm::normalize(glm::cross(glm::normalize(farVec + upDir * halfH), rightDir));
        topPlane.distance = -glm::dot(topPlane.normal, position);

        Plane& rightPlane = planes[Frustum::PLANE_IDX_RIGHT];
        rightPlane.normal = glm::normalize(glm::cross(glm::normalize(farVec + rightDir * halfW), -upDir));
        rightPlane.distance = -glm::dot(rightPlane.normal, position);

        Plane& bottomPlane = planes[Frustum::PLANE_IDX_BOTTOM];
        bottomPlane.normal = glm::normalize(glm::cross(glm::normalize(farVec - upDir * halfH), -rightDir));
        bottomPlane.distance = -glm::dot(bottomPlane.normal, position);

        Plane& nearPlane = planes[Frustum::PLANE_IDX_NEAR];
        nearPlane.normal = forwardDir;
        nearPlane.distance = -glm::dot(nearPlane.normal, position + forwardDir * zNear);

        Plane& farPlane = planes[Frustum::PLANE_IDX_FAR];
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


    const Plane& Frustum::GetPlane(size_t index) const
    {
        MATH_ASSERT(index < PLANE_COUNT);
        return planes[index];
    }
}