#ifndef MATH_BITS_H
#define MATH_BITS_H


#define SET_BIT(BIT)             (1u << (uint)(BIT))
#define IS_FLAG_SET(VALUE, MASK) (((uint)(VALUE) & (uint)(MASK)) != 0u)
#define IS_BIT_SET(VALUE, BIT)   IS_FLAG_SET((uint)(VALUE), SET_BIT(BIT))


float4 UnpackRGBA8(uint color)
{
    const float r = float((color >> 24) & 0xFF) / 255.f;
    const float g = float((color >> 16) & 0xFF) / 255.f;
    const float b = float((color >>  8) & 0xFF) / 255.f;
    const float a = float((color >>  0) & 0xFF) / 255.f;

    return float4(r, g, b, a);
}

#endif