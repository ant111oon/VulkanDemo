#ifndef MATH_CULLING_H
#define MATH_CULLING_H


struct SPHERE
{
    float3 position;
    float radius;
};


struct AABB
{
    float3 min;
    float3 max;
};


struct FRUSTUM_PLANE
{
    float3 normal;
    float distance;
};


struct FRUSTUM
{
    bool IsIntersect(in SPHERE sphere)
    {
        for (uint i = 0; i < 6; ++i) {
            FRUSTUM_PLANE plane = planes[i];

            if (dot(plane.normal, sphere.position) + plane.distance < -sphere.radius) {
                return false;
            }
        }

        return true;
    }

    bool IsIntersect(in AABB aabb)
    {
        for (uint i = 0; i < 6; ++i) {
            FRUSTUM_PLANE plane = planes[i];

            const float d =
                max(aabb.min.x * plane.normal.x, aabb.max.x * plane.normal.x) +
                max(aabb.min.y * plane.normal.y, aabb.max.y * plane.normal.y) +
                max(aabb.min.z * plane.normal.z, aabb.max.z * plane.normal.z) -
                plane.distance;

            if (d < 0.f) {
                return false;
            }
        }

        return true;
    }

    FRUSTUM_PLANE planes[6];
};


#endif