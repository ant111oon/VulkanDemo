import "half_utils.fx";
import "registers_common.fx";

import "test_vsin.fx";
import "test_vsout.fx";


[shader("vertex")]
TEST_VS_OUTPUT main(uint vid : SV_VertexID)
{
    TEST_VS_OUTPUT result = (TEST_VS_OUTPUT)0;

    TEST_INPUT_DATA inputData = PrepareInputData(vid);

    const COMMON_RENDER_INFO renderInfo = COMMON_RENDER_INFOS[TEST_REGISTRY.RENDER_INFO_IDX];

    const COMMON_TRANSFORM trs = COMMON_TRANSFORMS[renderInfo.TRANSFORM_IDX];
    const float4x4 wMatr = float4x4(
        trs.MATR[0],
        trs.MATR[1],
        trs.MATR[2], 
        float4(0.f, 0.f, 0.f, 1.f)
    );
    
    const float4 wpos = mul(wMatr, float4(inputData.position, 1.f));

    result.hpos = mul(COMMON_CB.COMMON_VIEW_PROJ_MATRIX, wpos); 
    result.lnorm = inputData.lnorm;
    result.texCoord = inputData.texCoord;

    return result; 
}