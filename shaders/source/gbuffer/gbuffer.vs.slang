#include "registers/gbuffer_registers.slang"

#include "pass/gbuffer/vsin.slang"
#include "pass/gbuffer/vsout.slang"


uint GetGBufferInstIdx(uint drawID)
{
#ifndef ENV_RELEASE
    if (!COMMON_DBG_IS_GEOM_INDIRECT_DRAW_ENABLED) {
        return GBUFFER_INST_INFO_IDX;
    } else
#endif
    {
        return (bool)GBUFFER_IS_AKILL_PASS ? GBUFFER_AKILL_INST_INFO_IDS[drawID] : GBUFFER_OPAQUE_INST_INFO_IDS[drawID];
    }
}


[shader("vertex")]
GBUFFER_VS_OUTPUT main(uint vertID : SV_VertexID, uint instID : SV_VulkanInstanceID, uint drawID : SV_DrawIndex)
{
    GBUFFER_VS_OUTPUT result = (GBUFFER_VS_OUTPUT)0;

    const uint instIdx = GetGBufferInstIdx(drawID);

    const COMMON_INST_DATA instData = COMMON_INST_DATA_BUFFER[instIdx];

    GBUFFER_INPUT_DATA inputData = PrepareGBufferVSInputData(vertID);

    const float4x4 wMatr = COMMON_TRANSFORMS[instData.TRANSFORM_IDX];
    const float3 wpos = mul(wMatr, float4(inputData.position, 1.f)).xyz;

    result.data.lpos = inputData.position;
    result.data.wpos = wpos;
    result.data.lnorm = inputData.lnorm;
    result.data.uv = inputData.uv;
    result.data.tangent = inputData.tangent;

    result.flatData.instID = instIdx;

    result.hpos = mul(COMMON_CB.VIEW_PROJ_MATRIX, float4(wpos, 1.f));

    return result; 
}