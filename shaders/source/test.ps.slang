
struct COMMON_CB_DATA
{
    float3 color;
    float PAD0;
};

[vk::binding(0, 0)] ConstantBuffer<COMMON_CB_DATA> COMMON_CB;


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

    result.color = float4(input.color.rgb * COMMON_CB.color, 1.f);
    
    return result;
}