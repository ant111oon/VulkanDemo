float HalfBitsToFloat(uint packed)
{
    uint sign = (packed >> 15) & 0x1;
    uint exponent = (packed >> 10) & 0x1F; // Extract exponent (5 bits)
    uint fraction = packed & 0x3FF;        // Extract fraction (10 bits)

    if (exponent == 0) {
        if (fraction == 0) {
            return sign == 0 ? 0.f : -0.f;
        } else {
            exponent = 1; // Handle subnormal numbers (denormalized floats)
        }
    }

    // If the exponent is 31, it could be infinity or NaN
    if (exponent == 31) {
        if (fraction == 0) {
            return sign == 0 ? float(1.f / 0.f) : float(-1.f / 0.f); // Infinity
        } else {
            return asfloat(0x7FC00000); // NaN 
        }
    }

    // Normalize the exponent to the proper range
    exponent = exponent - 15 + 127; // Bias conversion (15 for half-float, 127 for single-precision float)

    // Assemble the final 32-bit float
    const uint result = (sign << 31) | (exponent << 23) | (fraction << 13);

    return asfloat(result);
}


float2 UnpackHalf2x16(uint packed)
{
    const uint lowBits = packed & 0xFFFF;
    const uint highBits = packed >> 16;

    return float2(HalfBitsToFloat(lowBits), HalfBitsToFloat(highBits));
}