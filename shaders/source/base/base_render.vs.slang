#include "registers/common_registers.slang"

#include "base/vsin.slang"
#include "base/vsout.slang"


[shader("vertex")]
BASE_VS_OUTPUT main(uint vertID : SV_VertexID, uint instID : SV_VulkanInstanceID, uint drawID : SV_DrawIndex)
{
    BASE_VS_OUTPUT result = (BASE_VS_OUTPUT)0;

#ifdef ENV_DEBUG
    const uint instIdx = COMMON_DBG_IS_MESH_INDIRECT_DRAW ? BASE_RENDER_INDIRECT_DRAW_CMDS_UAV[drawID].INSTANCE_INFO_IDX : BASE_RENDER_REGISTRY.INST_INFO_IDX;
#else
    const uint instIdx = BASE_RENDER_INDIRECT_DRAW_CMDS_UAV[drawID].INSTANCE_INFO_IDX;
#endif

    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[instIdx];
    const COMMON_TRANSFORM transform = COMMON_TRANSFORMS[instInfo.TRANSFORM_IDX];

    const float3x4 wMatr = float3x4(transform.MATR[0], transform.MATR[1], transform.MATR[2]);

    BASE_INPUT_DATA inputData = PrepareBaseVSInputData(vertID);

    const float3 wpos = mul(wMatr, float4(inputData.position, 1.f)); 

    result.hpos = mul(COMMON_CB.COMMON_VIEW_PROJ_MATRIX, float4(wpos, 1.f)); 
    result.data.lnorm = inputData.lnorm;
    result.data.texCoord = inputData.texCoord;

    result.flatData.instID = instIdx;

    return result; 
}