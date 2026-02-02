#ifndef MATH_FRUSTUM_H
#define MATH_FRUSTUM_H


struct SPHERE
{
    float3 position;
    float radius;
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

    FRUSTUM_PLANE planes[6];
};


#endif