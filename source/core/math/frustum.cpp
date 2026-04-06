#include "pch.h"

#include "frustum.h"


namespace math
{
    Plane::Plane(const glm::vec3& norm, float dist)
        : normal(norm), distance(dist)
    {
        MATH_ASSERT(IsNormalized(normal));
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
}