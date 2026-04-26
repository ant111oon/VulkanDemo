#ifndef MATH_CONTRIBUTION_CULLING_H
#define MATH_CONTRIBUTION_CULLING_H

#include "geom_primitives.slang"


// Returns false if object pixel contribution is less or equal than certain value
// sbox - screen uv box
// vpSize - size of projection area in pixels
bool Contribution_Visibility(in SBOX sbox, in uint2 vpSize, in float2 falloff)
{
    const float2 size = sbox.GetDeltaUV() * (float2)vpSize;
    return all(size > falloff);
}

#endif