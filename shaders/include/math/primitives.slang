#ifndef PRIMITIVE_H
#define PRIMITIVE_H

#include "math.slang"


struct AABB
{
    float3 GetCenter() { return (min + max) * 0.5f; }
    float3 GetExtent() { return (max - min) * 0.5f; }

    float3 min;
    float3 max;
};


struct SPHERE
{
    float3 position;
    float radius;
};


#endif