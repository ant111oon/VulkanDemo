#ifndef MATH_BITS_PACK_UNPACK_H
#define MATH_BITS_PACK_UNPACK_H


float4 UnpackRGBA8(uint color)
{
    const float r = float((color >> 24) & 0xFF) / 255.f;
    const float g = float((color >> 16) & 0xFF) / 255.f;
    const float b = float((color >>  8) & 0xFF) / 255.f;
    const float a = float((color >>  0) & 0xFF) / 255.f;

    return float4(r, g, b, a);
}

#endif