#define DBG_DRAW_TRIANGLES_PASS

#include "registers/dbg_primitives_registers.slang"

#include "pass/dbg_primitives/vsin.slang"
#include "pass/dbg_primitives/vsout.slang"


[shader("vertex")]
GPU_DbgPrimVsOutput main(uint vertID : SV_VertexID, uint instID : SV_VulkanInstanceID)
{
    GPU_DbgPrimInput inputData = PrepareDbgDrawVSInputData(instID, vertID);

    GPU_DbgPrimVsOutput result = (GPU_DbgPrimVsOutput)0;

    result.data.color = inputData.color;
    result.hpos = mul(COMMON_MAIN_CAM_VIEW_PROJ_MATRIX, float4(inputData.wPos, 1.f));

    return result; 
}