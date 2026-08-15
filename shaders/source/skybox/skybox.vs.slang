#include "registers/skybox_registers.slang"

#include "pass/skybox/vsin.slang"
#include "pass/skybox/vsout.slang"


[shader("vertex")]
GPU_SkyboxVsOutput main(uint vertID : SV_VertexID)
{
    GPU_SkyboxVsOutput result = (GPU_SkyboxVsOutput)0;

    GPU_SkyboxInput inputData = PrepareSkyboxVSInputData(vertID);

    const float3 viewPos = mul((float3x3)COMMON_MAIN_CAM_VIEW_MATRIX, inputData.position);
    result.hpos = mul(COMMON_MAIN_CAM_PROJ_MATRIX, float4(viewPos, 1.f));

#ifdef ENV_REVERSED_Z
    result.hpos.z = 0.f;
#else
    result.hpos.z = result.hpos.w;
#endif

    result.data.uvw = inputData.position;

    return result; 
}