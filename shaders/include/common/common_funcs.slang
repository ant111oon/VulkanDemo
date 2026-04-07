#ifndef COMMON_FUNCS_H
#define COMMON_FUNCS_H


float3 GetScreenQuadVertNDC(in uint vertID)
{
#ifdef ENV_API_VULKAN
    static const float BOTTOM_SCREEN_NDC_Y = 1.f;
    static const float TOP_SCREEN_NDC_Y = -1.f;
#else
    static const float BOTTOM_SCREEN_NDC_Y = -1.f;
    static const float TOP_SCREEN_NDC_Y = 1.f;
#endif

    static const float2 QUAD_VERTS_XY[] =
    {
        float2(-1.f, BOTTOM_SCREEN_NDC_Y),
        float2(1.f,  TOP_SCREEN_NDC_Y),
        float2(-1.f, TOP_SCREEN_NDC_Y),

        float2(-1.f, BOTTOM_SCREEN_NDC_Y),
        float2(1.f,  BOTTOM_SCREEN_NDC_Y),
        float2(1.f,  TOP_SCREEN_NDC_Y)
    };

#ifdef ENV_REVERSED_Z
    static const float z = 0.f;
#else
    static const float z = 1.f;
#endif

    return float3(QUAD_VERTS_XY[vertID], z);
}

#endif