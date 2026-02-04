#include "pass/deferred_lighting/vsin.slang"
#include "pass/deferred_lighting/vsout.slang"

#include "common/common_funcs.slang"


[shader("vertex")]
DEFERRED_LIGHTING_VS_OUTPUT main(uint vertID : SV_VertexID)
{
    DEFERRED_LIGHTING_VS_OUTPUT result = (DEFERRED_LIGHTING_VS_OUTPUT)0;

    result.hpos = float4(GetQuadMeshVert(vertID), 1.f);

    return result;
}