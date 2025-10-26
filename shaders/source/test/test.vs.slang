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

    result.hpos = mul(float4(inputData.position, 1.f), COMMON_CB.COMMON_VIEW_PROJ_MATRIX);

    return result;
}