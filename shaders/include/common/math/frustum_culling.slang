#ifndef MATH_FRUSTUM_CULLING_H
#define MATH_FRUSTUM_CULLING_H

#include "geom_primitives.slang"


bool Frustum_Visibility(in FRUSTUM frustum, in AABB aabb)
{
    return frustum.IsIntersect(aabb);
}

#endif