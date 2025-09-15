

struct TEST_PS_INPUT
{
    [[vk::location(0)]] float3 lnorm;
    [[vk::location(1)]] float2 texCoord;

    float4 hpos : SV_Position;
};


struct TEST_PS_OUTPUT
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
TEST_PS_OUTPUT main(TEST_PS_INPUT input)
{
    TEST_PS_OUTPUT result = (TEST_PS_OUTPUT)0;

    result.color = float4(input.lnorm, 1.f);
    
    return result;
}