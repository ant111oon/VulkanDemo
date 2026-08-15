#include "registers/gbuffer_registers.slang"

#include "pass/gbuffer/vsin.slang"
#include "pass/gbuffer/vsout.slang"


[shader("vertex")]
GPU_GBufferVsOutput main(uint vertID : SV_VertexID, uint instIndex : SV_VulkanInstanceID, uint drawID : SV_DrawIndex)
{
    GPU_GBufferInput inputData = PrepareGBufferVSInputData(vertID);

    const uint instID = GBUFFER_INST_ID_QUEUE[instIndex];
    const GPU_GeomInst inst = COMMON_INST_BUFFER[instID];

    const float3 wpos = mul(inst.matrWCS, float4(inputData.position, 1.f)).xyz;

    GPU_GBufferVsOutput result = (GPU_GBufferVsOutput)0;

    result.data.lpos = inputData.position;
    result.data.wpos = wpos;
    result.data.lnorm = inputData.lnorm;
    result.data.uv = inputData.uv;
    result.data.tangent = inputData.tangent;

    result.flatData.instID = instID;

    result.hpos = mul(COMMON_MAIN_CAM_VIEW_PROJ_MATRIX, float4(wpos, 1.f));

    return result; 
}