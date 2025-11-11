#include "common_registers.slang"
#include "common_math.slang"

#include "base_vsin.slang"
#include "base_vsout.slang"


[shader("vertex")]
BASE_VS_OUTPUT main(uint vertID : SV_VertexID, uint instID : SV_VulkanInstanceID)
{
    BASE_VS_OUTPUT result = (BASE_VS_OUTPUT)0;

    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[instID];
    const COMMON_TRANSFORM transform = COMMON_TRANSFORMS[instInfo.TRANSFORM_IDX];

    const float3x4 wMatr = float3x4(transform.MATR[0], transform.MATR[1], transform.MATR[2]);

    BASE_INPUT_DATA inputData = PrepareBaseVSInputData(vertID);

    const float3 wpos = mul(wMatr, float4(inputData.position, 1.f));

    result.hpos = mul(COMMON_CB.COMMON_VIEW_PROJ_MATRIX, float4(wpos, 1.f)); 
    result.data.lnorm = inputData.lnorm;
    result.data.texCoord = inputData.texCoord;

    result.flatData.instID = instID;

    return result; 
}