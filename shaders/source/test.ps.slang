
struct TEST_PS_INPUT
{
    [[vk::location(0)]] float4 color;
    [[vk::location(1)]] float2 texCooord;
};


struct TEST_PS_OUTPUT
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
TEST_PS_OUTPUT main(TEST_PS_INPUT input)
{
    TEST_PS_OUTPUT result = (TEST_PS_OUTPUT)0;
    
    result.color = input.color;
    
    return result;
}