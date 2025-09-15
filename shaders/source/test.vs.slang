
struct BINDLESS_REGISTRY
{
    uint4* VERTEX_DATA;
};


[[vk::push_constant]] BINDLESS_REGISTRY REGISTRY;


struct COMMON_CB_DATA
{
    float4x4 COMMON_VIEW_MATRIX;
    float4x4 COMMON_PROJ_MATRIX;
    float4x4 COMMON_VIEW_PROJ_MATRIX;
};

[vk::binding(0, 0)] ConstantBuffer<COMMON_CB_DATA> COMMON_CB;


struct INPUT_DATA
{
    float3 position;
    float3 lnorm;
    float2 texCoord;
};

// Convert a half-precision (16-bit) float to a single-precision (32-bit) float
float halfToFloat(uint h)
{
    // Half-precision (16-bit) float to 32-bit float conversion
    uint sign = (h >> 15) & 0x1;      // Extract sign (1 bit)
    uint exponent = (h >> 10) & 0x1F; // Extract exponent (5 bits)
    uint fraction = h & 0x3FF;        // Extract fraction (10 bits)

    // If the exponent is 0, it's a subnormal number or zero
    if (exponent == 0)
    {
        if (fraction == 0)
        {
            return sign == 0 ? 0.0 : -0.0; // Zero (signed)
        } else {
            // Handle subnormal numbers (denormalized floats)
            exponent = 1;
        }
    }
    // If the exponent is 31, it could be infinity or NaN
    if (exponent == 31)
    {
        if (fraction == 0) {
            return sign == 0 ? float(1.0 / 0.0) : float(-1.0 / 0.0); // Infinity
        } else {
            return asfloat(0x7FC00000); // NaN
        }
    }

    // Normalize the exponent to the proper range
    exponent = exponent - 15 + 127; // Bias conversion (15 for half-float, 127 for single-precision float)

    // Assemble the final 32-bit float
    uint result = (sign << 31) | (exponent << 23) | (fraction << 13);
    return asfloat(result); // Convert the result to a float
}


// Manually unpack a packed 32-bit uint to two half-floats
float2 unpackHalf2x16(uint packed)
{
    // Extract the low and high 16 bits
    uint lowBits = packed & 0xFFFFu; // lower 16 bits
    uint highBits = packed >> 16;    // upper 16 bits

    // Convert each half-precision value to float
    float2 result;
    result.x = halfToFloat(lowBits);  // Convert the lower half-float to float
    result.y = halfToFloat(highBits); // Convert the upper half-float to float

    return result;
}




#define UNPACK_VERTEX_LPOS(VERTEX_OFFSET)     float3(unpackHalf2x16(REGISTRY.VERTEX_DATA[VERTEX_OFFSET][0]), unpackHalf2x16(REGISTRY.VERTEX_DATA[VERTEX_OFFSET][1]).x)
#define UNPACK_VERTEX_LNORM(VERTEX_OFFSET)    float3(unpackHalf2x16(REGISTRY.VERTEX_DATA[VERTEX_OFFSET][1]).y, unpackHalf2x16(REGISTRY.VERTEX_DATA[VERTEX_OFFSET][2]))
#define UNPACK_VERTEX_TEXCOORD(VERTEX_OFFSET) unpackHalf2x16(REGISTRY.VERTEX_DATA[VERTEX_OFFSET][3])


INPUT_DATA PrepareInputData(uint vertIdx)
{
    INPUT_DATA inputData;

    static const uint VERTEX_DATA_SIZE_UI4 = 1;
    const uint offset = vertIdx * VERTEX_DATA_SIZE_UI4;

    inputData.position = UNPACK_VERTEX_LPOS(offset);
    inputData.lnorm    = UNPACK_VERTEX_LNORM(offset);
    inputData.texCoord = UNPACK_VERTEX_TEXCOORD(offset);

    return inputData;
}


struct TEST_VS_OUTPUT
{
    [[vk::location(0)]] float3 lnorm;
    [[vk::location(1)]] float2 texCoord;

    float4 hpos : SV_Position;
};


[shader("vertex")]
TEST_VS_OUTPUT main(uint vid : SV_VertexID)
{
    TEST_VS_OUTPUT result = (TEST_VS_OUTPUT)0;

    INPUT_DATA inputData = PrepareInputData(vid);

    result.lnorm = inputData.lnorm;
    result.texCoord = inputData.texCoord;

    result.hpos = mul(float4(inputData.position, 1.f), COMMON_CB.COMMON_VIEW_PROJ_MATRIX);

    return result;
}