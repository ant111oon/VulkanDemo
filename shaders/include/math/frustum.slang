#ifndef FRUSTUM_H
#define FRUSTUM_H

#include "math.slang"
#include "primitives.slang"


struct FRUSTUM_PLANE
{
    float3 normal;
    float distance;
};


struct FRUSTUM
{
    bool IsIntersect(in AABB aabb)
    {
        for (uint i = 0; i < 6; ++i) {
            FRUSTUM_PLANE plane = planes[i];

            float3 positive = M3D_ZEROF3;
            positive.x = (plane.normal.x >= 0.f) ? aabb.max.x : aabb.min.x;
            positive.y = (plane.normal.y >= 0.f) ? aabb.max.y : aabb.min.y;
            positive.z = (plane.normal.z >= 0.f) ? aabb.max.z : aabb.min.z;

            if (dot(plane.normal, positive) + plane.distance < 0.f) {
                return false;
            }
        }

        return true;
    }

    FRUSTUM_PLANE planes[6];
};


#endif