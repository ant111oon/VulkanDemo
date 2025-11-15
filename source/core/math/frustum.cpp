#include "pch.h"

#include "frustum.h"


namespace math
{
    Plane::Plane(const glm::vec3& norm, float dist)
        : normal(norm), distance(dist)
    {
        MATH_ASSERT(IsNormalized(normal));
    }
}