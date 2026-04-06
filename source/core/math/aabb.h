#pragma once

#include "math.h"


namespace math
{
    struct AABB
    {
        AABB() = default;
        AABB(const glm::float3& min, const glm::float3& max);

        glm::float3 min;
        glm::float3 max;
    };
}