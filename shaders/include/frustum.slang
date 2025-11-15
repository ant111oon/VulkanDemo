#ifndef FRUSTUM_H
#define FRUSTUM_H

#include "primitives.slang"
#include "const_math.slang"


struct FRUSTUM_PLANE
{
    float3 normal;
    float distance;
};


static const uint COMMON_FRUSTUM_PLANES_COUNT = 6;


struct FRUSTUM
{
    bool IsIntersect(in AABB aabb)
    {
        float minDot = M3D_FLT_MAX, maxDot = -M3D_FLT_MAX;

        for (uint i = 0; i < COMMON_FRUSTUM_PLANES_COUNT; ++i) {
            minDot = dot(aabb.min, planes[i].normal) + planes[i].distance;
            maxDot = dot(aabb.max, planes[i].normal) + planes[i].distance;

            if (minDot < 0.f && maxDot < 0.f) {
                return false;
            }
        }

        return true;
    }

    FRUSTUM_PLANE planes[COMMON_FRUSTUM_PLANES_COUNT];
};


#endif