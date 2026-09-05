#include "registers/common_registers.slang"

#include "pass/depth/vsin.slang"
#include "pass/depth/vsout.slang"


[shader("vertex")]
GPU_DepthVsOutput main(uint vertID : SV_VertexID, uint instIndex : SV_VulkanInstanceID, uint drawID : SV_DrawIndex)
{
    GPU_DepthInput inputData = PrepareDepthVSInputData(vertID, DEPTH_IS_AKILL_PASS);

    const uint instID = DEPTH_INST_ID_QUEUE[instIndex];
    const GPU_GeomInst inst = COMMON_INST_BUFFER[instID];

    const float3 wpos = mul(inst.matrWCS, float4(inputData.position, 1.f)).xyz;

    GPU_DepthVsOutput result = (GPU_DepthVsOutput)0;

    result.data.uv = inputData.uv;

    result.flatData.instID = instID;

    result.hpos = mul(DEPTH_IS_CSM_PASS ? COMMON_CSM_VIEW_PROJ_MATRIX(DEPTH_CSM_CASCADE) : COMMON_MAIN_CAM_VIEW_PROJ_MATRIX, float4(wpos, 1.f)); 

    return result; 
}