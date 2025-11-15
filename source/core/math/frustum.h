#pragma once

#include "math.h"

#include <array>


namespace math
{
    struct Plane
    {
        glm::vec3 normal = { 0.f, 1.f, 0.f };
        float distance = 0.f;
    };
    
    
    struct Frustum
    {
        
    };
}
