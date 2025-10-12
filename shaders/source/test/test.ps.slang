import "test_vsout.fx";
import "registers_common.fx";


struct TEST_PS_OUTPUT
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
TEST_PS_OUTPUT main(TEST_VS_OUTPUT input)
{
    TEST_PS_OUTPUT result = (TEST_PS_OUTPUT)0;

#ifdef ENV_USE_MESHLET_PIPELINE
    result.color = float4(input.color, 1.f);
#else
    result.color = float4(input.lnorm, 1.f);
#endif

    return result;
}