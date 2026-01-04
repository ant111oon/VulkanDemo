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


float2 UVToNDC(in float2 uv)
{
    return 2.f * uv - 1.f;
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