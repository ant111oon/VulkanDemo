#include "pch.h"

#include "aabb.h"


namespace math
{
    AABB::AABB(const glm::float3& min, const glm::float3& max)
        : min(min), max(max)
    {
        MATH_ASSERT(glm::all(glm::lessThan(min, max)));
    }
}