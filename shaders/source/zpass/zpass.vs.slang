#include "registers/common_registers.slang"

#include "pass/zpass/vsin.slang"
#include "pass/zpass/vsout.slang"


[shader("vertex")]
GPU_ZPassVsOutput main(uint vertID : SV_VertexID, uint instIndex : SV_VulkanInstanceID, uint drawID : SV_DrawIndex)
{
    GPU_ZPassInput inputData = PrepareZPassVSInputData(vertID, ZPASS_IS_AKILL_PASS);

    const uint instID = ZPASS_INST_ID_QUEUE[instIndex];
    const GPU_GeomInst inst = COMMON_INST_BUFFER[instID];

    const float3 wpos = mul(inst.matrWCS, float4(inputData.position, 1.f)).xyz;

    GPU_ZPassVsOutput result = (GPU_ZPassVsOutput)0;

    result.data.uv = inputData.uv;

    result.flatData.instID = instID;

    result.hpos = mul(ZPASS_IS_CSM_PASS ? COMMON_CSM_VIEW_PROJ_MATRIX(ZPASS_CSM_CASCADE) : COMMON_MAIN_CAM_VIEW_PROJ_MATRIX, float4(wpos, 1.f)); 

    return result; 
}