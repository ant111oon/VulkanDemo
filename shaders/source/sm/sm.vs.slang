#include "registers/common_registers.slang"

#include "pass/sm/vsin.slang"
#include "pass/sm/vsout.slang"

#include "common/sm_incl.slang"


[shader("vertex")]
SM_VS_OUTPUT main(uint vertID : SV_VertexID, uint instIndex : SV_VulkanInstanceID, uint drawID : SV_DrawIndex)
{
    SM_VS_OUTPUT result = (SM_VS_OUTPUT)0;

    SM_INPUT_DATA inputData = PrepareSMPassVSInputData(vertID, CSM_IS_AKILL_PASS);

    const uint queue = SMGetQueueIndex(CSM_CASCADE_IDX, CSM_QUEUE);

    const uint instID = CSM_INST_ID_QUEUE[queue][instIndex];
    const COMMON_INST inst = COMMON_INST_BUFFER[instID];

    const float3 wpos = mul(inst.MATR_WCS, float4(inputData.position, 1.f)).xyz;

    result.data.uv = inputData.uv;

    result.flatData.instID = instID;

    result.hpos = mul(CSM_CASCADE_DATA(CSM_CASCADE_IDX).VIEW_PROJ_MATRIX, float4(wpos, 1.f)); 

    return result; 
}