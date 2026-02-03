#include "registers/common_registers.slang"

#include "pass/zpass/vsin.slang"
#include "pass/zpass/vsout.slang"


uint GetZPassInstIdx(uint drawID)
{
#ifdef ENV_DEBUG
    if (!COMMON_DBG_IS_MESH_INDIRECT_DRAW_ENABLED) {
        return PUSH_CONSTS.INST_INFO_IDX;
    } else
#endif
    {
        return (bool)PUSH_CONSTS.IS_AKILL_PASS ? ZPASS_AKILL_INST_INFO_IDS[drawID] : ZPASS_OPAQUE_INST_INFO_IDS[drawID];
    }
}


[shader("vertex")]
ZPASS_VS_OUTPUT main(uint vertID : SV_VertexID, uint instID : SV_VulkanInstanceID, uint drawID : SV_DrawIndex)
{
    ZPASS_VS_OUTPUT result = (ZPASS_VS_OUTPUT)0;

    const uint instIdx = GetZPassInstIdx(drawID);

    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[instIdx];

    ZPASS_INPUT_DATA inputData = PrepareZPassVSInputData(vertID);
    
    const float4x4 wMatr = COMMON_TRANSFORMS[instInfo.TRANSFORM_IDX];
    const float3 wpos = mul(wMatr, float4(inputData.position, 1.f)).xyz;

    result.data.uv = inputData.uv;

    result.flatData.instID = instIdx;

    result.hpos = mul(COMMON_CB.VIEW_PROJ_MATRIX, float4(wpos, 1.f)); 

    return result; 
}