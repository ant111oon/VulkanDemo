#include "registers/common_registers.slang"

#include "zpass/vsin.slang"
#include "zpass/vsout.slang"


[shader("vertex")]
ZPASS_VS_OUTPUT main(uint vertID : SV_VertexID, uint instID : SV_VulkanInstanceID, uint drawID : SV_DrawIndex)
{
    ZPASS_VS_OUTPUT result = (ZPASS_VS_OUTPUT)0;

#ifdef ENV_DEBUG
    const uint instIdx = COMMON_DBG_IS_MESH_INDIRECT_DRAW_ENABLED ? ZPASS_INDIRECT_DRAW_CMDS_UAV[drawID].INSTANCE_INFO_IDX : ZPASS_REGISTRY.INST_INFO_IDX;
#else
    const uint instIdx = ZPASS_INDIRECT_DRAW_CMDS_UAV[drawID].INSTANCE_INFO_IDX;
#endif

    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[instIdx];
    const COMMON_TRANSFORM transform = COMMON_TRANSFORMS[instInfo.TRANSFORM_IDX];

    const float3x4 wMatr = float3x4(transform.MATR[0], transform.MATR[1], transform.MATR[2]);

    ZPASS_INPUT_DATA inputData = PrepareZPassVSInputData(vertID);

    const float3 wpos = mul(wMatr, float4(inputData.position, 1.f)); 

    result.hpos = mul(COMMON_CB.COMMON_VIEW_PROJ_MATRIX, float4(wpos, 1.f)); 

    return result; 
}