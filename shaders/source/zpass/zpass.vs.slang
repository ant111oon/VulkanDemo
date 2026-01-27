#include "registers/common_registers.slang"

#include "pass/zpass/vsin.slang"
#include "pass/zpass/vsout.slang"


[shader("vertex")]
ZPASS_VS_OUTPUT main(uint vertID : SV_VertexID, uint instID : SV_VulkanInstanceID, uint drawID : SV_DrawIndex)
{
    ZPASS_VS_OUTPUT result = (ZPASS_VS_OUTPUT)0;

#ifdef ENV_DEBUG
    const uint instIdx = COMMON_DBG_IS_MESH_INDIRECT_DRAW_ENABLED ? ZPASS_INST_INFO_IDS[drawID] : PUSH_CONSTS.INST_INFO_IDX;
#else
    const uint instIdx = ZPASS_INST_INFO_IDS[drawID];
#endif

    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[instIdx];

    ZPASS_INPUT_DATA inputData = PrepareZPassVSInputData(vertID);
    
    const float4x4 wMatr = COMMON_TRANSFORMS[instInfo.TRANSFORM_IDX];
    const float3 wpos = mul(wMatr, float4(inputData.position, 1.f)).xyz; 

    result.hpos = mul(COMMON_CB.VIEW_PROJ_MATRIX, float4(wpos, 1.f)); 

    return result; 
}