#ifndef MATH_CULLING_H
#define MATH_CULLING_H

#include "common/math/constants.slang"


struct SPHERE
{
    float3 position;
    float radius;
};


struct AABB
{
    float3 GetCenter()
    {
        return (min + max) * 0.5f;
    }

    float3 GetSize()
    {
        return max - min;
    }

    float3 min;
    float3 max;
};


struct PLANE
{
    float3 normal;
    float distance;
};


struct FRUSTUM
{
    bool IsIntersect(in SPHERE sphere)
    {
        for (uint i = 0; i < 6; ++i) {
            PLANE plane = planes[i];

            if (dot(plane.normal, sphere.position) + plane.distance < -sphere.radius) {
                return false;
            }
        }

        return true;
    }

    bool IsIntersect(in AABB aabb)
    {
        for (uint i = 0; i < 6; ++i) {
            PLANE plane = planes[i];

            const float3 p = float3(
                (plane.normal.x >= 0.f) ? aabb.max.x : aabb.min.x,
                (plane.normal.y >= 0.f) ? aabb.max.y : aabb.min.y,
                (plane.normal.z >= 0.f) ? aabb.max.z : aabb.min.z
            );

            if (dot(plane.normal, p) + plane.distance < 0.f) {
                return false;
            }
        }

        return true;
    }

    PLANE planes[6];
};


#endif