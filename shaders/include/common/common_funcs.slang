#ifndef COMMON_FUNCS_H
#define COMMON_FUNCS_H

float3 GetQuadMeshVert(in uint vertID)
{
    static const float2 QUAD_VERTS[] =
    {
        float2(-1.f, 1.f),
        float2(1.f, -1.f),
        float2(-1.f, -1.f),

        float2(-1.f, 1.f),
        float2(1.f, 1.f),
        float2(1.f, -1.f)
    };

#ifdef ENV_REVERSED_Z
    static const float z = 0.f;
#else
    static const float z = 1.f;
#endif

    return float3(QUAD_VERTS[vertID], z);
}

#endif