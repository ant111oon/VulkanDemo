#include "registers/skybox_registers.slang"

#include "pass/skybox/vsin.slang"
#include "pass/skybox/vsout.slang"


[shader("vertex")]
SKYBOX_VS_OUTPUT main(uint vertID : SV_VertexID)
{
    SKYBOX_VS_OUTPUT result = (SKYBOX_VS_OUTPUT)0;

    SKYBOX_INPUT_DATA inputData = PrepareSkyboxVSInputData(vertID);

    const float3 viewPos = mul((float3x3)COMMON_CB.VIEW_MATRIX, inputData.position);
    result.hpos = mul(COMMON_CB.PROJ_MATRIX, float4(viewPos, 1.f));

#ifdef ENV_REVERSED_Z
    result.hpos.z = 0.f;
#else
    result.hpos.z = result.hpos.w;
#endif

    result.data.uvw = inputData.position;

    return result; 
}