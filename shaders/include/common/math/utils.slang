#ifndef MATH_UTILS_H
#define MATH_UTILS_H


float SRGBToLinear(in float color)
{
    // https://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
    return color * (color * (color * 0.305306011 + 0.682171111) + 0.012522878);
}


float3 SRGBToLinear(in float3 color)
{
    return float3(SRGBToLinear(color.r), SRGBToLinear(color.g), SRGBToLinear(color.b)); 
}


float LinearToSRGB(in float color)
{
    return color < 0.0031308f ? 12.92f * color : 1.055f * pow(color, 1.f / 2.4f) - 0.055f;
}


float3 LinearToSRGB(in float3 color)
{
    return float3(LinearToSRGB(color.r), LinearToSRGB(color.g), LinearToSRGB(color.b));
}


float3 LinearToSRGBFast(in float3 color)
{
    return pow(saturate(color), 1.f / 2.2f);
}


float3x3 CalcTBN(in float3 tang, in float3 norm)
{
    const float3 T = normalize(tang - dot(tang, norm) * norm);
    const float3 B = normalize(cross(norm, T));

    return float3x3(T, B, norm);
}


// Return depth in [near, far] range
float LinearizeDepth(in float depth, in float near, in float far)
{
#ifdef ENV_REVERSED_Z
    return (near * far) / (near + depth * (far - near));
#else
    return (near * far) / (far - depth * (far - near));
#endif
}


float LinearizeDepth01(in float depth, in float near, in float far)
{
    const float linear = LinearizeDepth(depth, near, far);
    return (linear - near) / (far - near);
}


float3 UVFaceToDir(in float2 uv, in uint face)
{
    switch (face) {
        case 0: return normalize(float3( 1.f,   uv.y, -uv.x)); // +X
        case 1: return normalize(float3(-1.f,   uv.y,  uv.x)); // -X
        case 2: return normalize(float3( uv.x,  1.f,  -uv.y)); // +Y
        case 3: return normalize(float3( uv.x, -1.f,   uv.y)); // -Y
        case 4: return normalize(float3( uv.x,  uv.y,  1.f));  // +Z
        case 5: return normalize(float3(-uv.x,  uv.y, -1.f));  // -Z
    }

    return float3(1.f, 0.f, 0.f);
}


float3 PixelCoordToUV(in uint3 coord, in uint3 size)
{
    size = max(size, uint3(1, 1, 1));

    // Pixel coords have (0, 0) in top left corner but UV (0, 0) is in bottom left corner
    return float3(coord.x, size.y - coord.y, coord.z) / size;
}


float2 PixelCoordToUV(in uint2 coord, in uint2 size)
{
    return PixelCoordToUV(uint3(coord, 0), uint3(size, 1)).xy;
}


float2 UVToNDC(in float2 uv)
{
    // Vulkan has Y NDC towards -Y
    return 2.f * float2(uv.x, 1.f - uv.y) - 1.f;
}


float4 NDCToView(in float3 NDC, in float4x4 invProjMatr)
{
    float4 result = mul(invProjMatr, float4(NDC, 1.f));
    return result /= result.w;
}


float3 ViewPosFromDepth(in float2 uv, in float depth, in float4x4 invProjMatr)
{
    return NDCToView(float3(UVToNDC(uv), depth), invProjMatr).xyz;
}


float3 WPosFromDepth(in float2 uv, in float depth, in float4x4 invViewMatr, in float4x4 invProjMatr)
{
    const float3 viewPos = ViewPosFromDepth(uv, depth, invProjMatr);
    return mul(invViewMatr, float4(viewPos, 1.f)).xyz;
}

#endif