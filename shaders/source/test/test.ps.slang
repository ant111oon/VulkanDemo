import "test_vsout.fx";
import "registers_common.fx";

import "registers_test.fx";


struct TEST_PS_OUTPUT
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
TEST_PS_OUTPUT main(TEST_VS_OUTPUT input)
{
    TEST_PS_OUTPUT result = (TEST_PS_OUTPUT)0;

    result.color = COMMON_MTL_TEXTURES[TEST_REGISTRY.TEX_IDX].Sample(COMMON_SAMPLER, input.texCoord);

    if (result.color.a <= 0.0001f) {
        discard;
    }
    
    return result;
}