#include "pass/deferred_lighting/vsin.slang"
#include "pass/deferred_lighting/vsout.slang"


static float2 QUAD_VERTS[] =
{
    float2(-1.f,  1.f),
    float2( 1.f, -1.f),
    float2(-1.f, -1.f),

    float2(-1.f,  1.f),
    float2( 1.f,  1.f),
    float2( 1.f, -1.f)
};


[shader("vertex")]
DEFERRED_LIGHTING_VS_OUTPUT main(uint vertID : SV_VertexID)
{
    DEFERRED_LIGHTING_VS_OUTPUT result = (DEFERRED_LIGHTING_VS_OUTPUT)0;

#ifdef ENV_REVERSED_Z
    const float z = 0.f;
#else
    const float z = 1.f;
#endif

    result.hpos = float4(QUAD_VERTS[vertID], z, 1.f);

    return result;
}