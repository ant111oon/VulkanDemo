#include "registers/common_registers.slang"

#include "pass/zpass/vsin.slang"
#include "pass/zpass/vsout.slang"


[shader("vertex")]
ZPASS_VS_OUTPUT main(uint vertID : SV_VertexID, uint instIndex : SV_VulkanInstanceID, uint drawID : SV_DrawIndex)
{
    ZPASS_VS_OUTPUT result = (ZPASS_VS_OUTPUT)0;

    ZPASS_INPUT_DATA inputData = PrepareZPassVSInputData(vertID, ZPASS_IS_AKILL_PASS);

    const uint instID = ZPASS_INST_ID_QUEUE[instIndex];
    const COMMON_INST inst = COMMON_INST_BUFFER[instID];

    const float3 wpos = mul(inst.MATR_WCS, float4(inputData.position, 1.f)).xyz;

    result.data.uv = inputData.uv;

    result.flatData.instID = instID;

    result.hpos = mul(COMMON_CB.VIEW_PROJ_MATRIX, float4(wpos, 1.f)); 

    return result; 
}