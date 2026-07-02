#include "registers/common_registers.slang"

#include "pass/csm/vsin.slang"
#include "pass/csm/vsout.slang"

#include "common/csm_incl.slang"


[shader("vertex")]
CSM_VS_OUTPUT main(uint vertID : SV_VertexID, uint instIndex : SV_VulkanInstanceID, uint drawID : SV_DrawIndex)
{
    CSM_VS_OUTPUT result = (CSM_VS_OUTPUT)0;

    CSM_INPUT_DATA inputData = PrepareCSMPassVSInputData(vertID, CSM_IS_AKILL_PASS);

    const uint queue = CSMGetBufferIndex(CSM_CASCADE_IDX, CSM_QUEUE);

    const uint instID = CSM_INST_ID_QUEUE[queue][instIndex];
    const COMMON_INST inst = COMMON_INST_BUFFER[instID];

    const float3 wpos = mul(inst.MATR_WCS, float4(inputData.position, 1.f)).xyz;

    result.data.uv = inputData.uv;

    result.flatData.instID = instID;

    result.hpos = mul(COMMON_CSM_VIEW_PROJ_MATRIX(CSM_CASCADE_IDX), float4(wpos, 1.f)); 

    return result; 
}