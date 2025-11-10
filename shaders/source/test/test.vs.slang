import common_registers;
import common_math;

import test_vsin;
import test_vsout;


[shader("vertex")]
TEST_VS_OUTPUT main(uint vertID : SV_VertexID , uint drawID : SV_DrawIndex)
{
    TEST_VS_OUTPUT result = (TEST_VS_OUTPUT)0;

    const uint instInfoID = IsFlagSet(COMMON_CB.COMMON_DBG_FLAGS, (uint)COMMON_DBG_FLAG_MASKS::USE_MESH_INDIRECT_DRAW) ? drawID : TEST_REGISTRY.INST_INFO_IDX;

    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[instInfoID];
    const COMMON_TRANSFORM transform = COMMON_TRANSFORMS[instInfo.TRANSFORM_IDX];

    const float3x4 wMatr = float3x4(transform.MATR[0], transform.MATR[1], transform.MATR[2]);

    TEST_INPUT_DATA inputData = PrepareInputData(vertID);

    const float3 wpos = mul(wMatr, float4(inputData.position, 1.f));

    result.hpos = mul(COMMON_CB.COMMON_VIEW_PROJ_MATRIX, float4(wpos, 1.f)); 
    result.data.lnorm = inputData.lnorm;
    result.data.texCoord = inputData.texCoord;

    result.flatData.instInfoID = instInfoID;

    return result; 
}