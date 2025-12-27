#include "registers/common_registers.slang"

#include "pass/gbuffer/vsin.slang"
#include "pass/gbuffer/vsout.slang"


[shader("vertex")]
GBUFFER_VS_OUTPUT main(uint vertID : SV_VertexID, uint instID : SV_VulkanInstanceID, uint drawID : SV_DrawIndex)
{
    GBUFFER_VS_OUTPUT result = (GBUFFER_VS_OUTPUT)0;

#ifdef ENV_DEBUG
    const uint instIdx = COMMON_DBG_IS_MESH_INDIRECT_DRAW_ENABLED ? GBUFFER_INDIRECT_DRAW_CMDS_UAV[drawID].INSTANCE_INFO_IDX : GBUFFER_REGISTRY.INST_INFO_IDX;
#else
    const uint instIdx = GBUFFER_INDIRECT_DRAW_CMDS_UAV[drawID].INSTANCE_INFO_IDX;
#endif

    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[instIdx];

    GBUFFER_INPUT_DATA inputData = PrepareGBufferVSInputData(vertID);

    const float4x4 wMatr = COMMON_TRANSFORMS[instInfo.TRANSFORM_IDX];
    const float3 wpos = mul(wMatr, float4(inputData.position, 1.f)).xyz;

    result.data.lpos = inputData.position;
    result.data.wpos = wpos;
    result.data.lnorm = inputData.lnorm;
    result.data.uv = inputData.uv;
    result.data.tangent = inputData.tangent;

    result.flatData.instID = instIdx;

    result.hpos = mul(COMMON_CB.COMMON_VIEW_PROJ_MATRIX, float4(wpos, 1.f));

    return result; 
}