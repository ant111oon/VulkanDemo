#ifndef MATH_HASH_H
#define MATH_HASH_H

#include "constants.slang"


uint Hash(in uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}


uint Hash(in uint2 v)
{
    uint h = Hash(v.x);
    h = Hash(h ^ v.y);
    return h;
}


uint Hash(in uint3 v)
{
    uint h = Hash(v.xy);
    h = Hash(h ^ v.z);
    return h;
}

#endif