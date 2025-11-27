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
        float minDot = M3D_FLT_MAX, maxDot = -M3D_FLT_MAX;

        for (uint i = 0; i < 6; ++i) {
            minDot = dot(aabb.min, planes[i].normal) + planes[i].distance;
            maxDot = dot(aabb.max, planes[i].normal) + planes[i].distance;

            if (minDot < 0.f && maxDot < 0.f) {
                return false;
            }
        }

        return true;
    }

    FRUSTUM_PLANE planes[6];
};


#endif