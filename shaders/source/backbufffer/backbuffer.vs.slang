#include "pass/backbuffer/vsin.slang"
#include "pass/backbuffer/vsout.slang"

#include "common/common_funcs.slang"


[shader("vertex")]
BACKBUFFER_VS_OUTPUT main(uint vertID : SV_VertexID)
{
    BACKBUFFER_VS_OUTPUT result = (BACKBUFFER_VS_OUTPUT)0;

    result.hpos = float4(GetQuadMeshVert(vertID), 1.f);

    return result;
}