import "half_utils.fx";
import "registers_common.fx";

import "test_vsin.fx";
import "test_vsout.fx";

[shader("vertex")]
TEST_VS_OUTPUT main(uint vertID : SV_VertexID , uint drawID : SV_DrawIndex)
{
    TEST_VS_OUTPUT result = (TEST_VS_OUTPUT)0;

    const uint rndInfoID = TEST_REGISTRY.RENDER_INFO_IDX > 0 ? TEST_REGISTRY.RENDER_INFO_IDX : drawID;

    TEST_INPUT_DATA inputData = PrepareInputData(vertID);

    const COMMON_RENDER_INFO renderInfo = COMMON_RENDER_INFOS[rndInfoID];

    const COMMON_TRANSFORM trs = COMMON_TRANSFORMS[renderInfo.TRANSFORM_IDX];
    const float3x4 wMatr = float3x4(
        trs.MATR[0],
        trs.MATR[1],
        trs.MATR[2] 
    );
    
    const float3 wpos = mul(wMatr, float4(inputData.position, 1.f));

    result.hpos = mul(COMMON_CB.COMMON_VIEW_PROJ_MATRIX, float4(wpos, 1.f)); 
    result.data.lnorm = inputData.lnorm;
    result.data.texCoord = inputData.texCoord;

    result.flatData.renderInfoID = rndInfoID;

    return result; 
}