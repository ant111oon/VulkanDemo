import "half_utils.fx";
import "registers_common.fx";

import "test_vsin.fx";
import "test_vsout.fx";


[shader("vertex")]
TEST_VS_OUTPUT main(uint vid : SV_VertexID)
{
    TEST_VS_OUTPUT result = (TEST_VS_OUTPUT)0;

    TEST_INPUT_DATA inputData = PrepareInputData(vid);

    result.lnorm = inputData.lnorm;
    result.texCoord = inputData.texCoord;

    const COMMON_RENDER_INFO renderInfo = COMMON_RENDER_INFOS[TEST_REGISTRY.RENDER_INFO_IDX];

    const float4x4 wMatr = COMMON_TRANSFORMS[renderInfo.TRANSFORM_IDX];

    const float4 wpos = mul(float4(inputData.position, 1.f), wMatr);
    const float4 viewPos = mul(wpos, COMMON_CB.COMMON_VIEW_MATRIX);

    result.hpos = mul(viewPos, COMMON_CB.COMMON_PROJ_MATRIX); 

    return result; 
}