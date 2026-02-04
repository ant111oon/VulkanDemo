#include "pass/post_processing/vsin.slang"
#include "pass/post_processing/vsout.slang"

#include "common/common_funcs.slang"


[shader("vertex")]
POST_PROC_VS_OUTPUT main(uint vertID : SV_VertexID)
{
    POST_PROC_VS_OUTPUT result = (POST_PROC_VS_OUTPUT)0;

    result.hpos = float4(GetQuadMeshVert(vertID), 1.f);

    return result;
}