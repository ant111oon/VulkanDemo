#ifndef COMMON_MATH_H
#define COMMON_MATH_H

#include "half_utils.slang"


bool IsFlagSet(uint value, uint mask)
{
    return (value & mask) != 0;
}


bool IsBitSet(uint value, uint bit)
{
    return IsFlagSet(value, uint(1) << bit);
}

#endif