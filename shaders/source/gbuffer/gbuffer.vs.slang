#include "registers/gbuffer_registers.slang"

#include "pass/gbuffer/vsin.slang"
#include "pass/gbuffer/vsout.slang"


[shader("vertex")]
GBUFFER_VS_OUTPUT main(uint vertID : SV_VertexID, uint instIndex : SV_VulkanInstanceID, uint drawID : SV_DrawIndex)
{
    GBUFFER_VS_OUTPUT result = (GBUFFER_VS_OUTPUT)0;

    GBUFFER_INPUT_DATA inputData = PrepareGBufferVSInputData(vertID);

    const uint instID = GBUFFER_INST_ID_QUEUE[instIndex];
    const COMMON_INST inst = COMMON_INST_BUFFER[instID];

    const float3x4 wMatr = COMMON_INST_TRANSFORMS[inst.TRANSFORM_IDX];
    const float3 wpos = mul(wMatr, float4(inputData.position, 1.f)).xyz;

    result.data.lpos = inputData.position;
    result.data.wpos = wpos;
    result.data.lnorm = inputData.lnorm;
    result.data.uv = inputData.uv;
    result.data.tangent = inputData.tangent;

    result.flatData.instID = instID;

    result.hpos = mul(COMMON_CB.VIEW_PROJ_MATRIX, float4(wpos, 1.f));

    return result; 
}