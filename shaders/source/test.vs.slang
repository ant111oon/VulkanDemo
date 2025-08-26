
struct BINDLESS_REGISTRY
{
    float4* VERTEX_DATA;
};


[[vk::push_constant]] BINDLESS_REGISTRY REGISTRY;


struct INPUT_DATA
{
    float2 position;
    float2 texCoord;
    float4 color;
};


INPUT_DATA PrepareInputData(uint vertIdx)
{
    INPUT_DATA inputData;

    static const uint INPUT_DATA_SIZE_F4 = 2;
    const uint offset = vertIdx * INPUT_DATA_SIZE_F4;

    inputData.position = REGISTRY.VERTEX_DATA[offset + 0].xy;
    inputData.texCoord = REGISTRY.VERTEX_DATA[offset + 0].zw;
    inputData.color    = REGISTRY.VERTEX_DATA[offset + 1].rgba;

    return inputData;
}


struct TEST_VS_OUTPUT
{
    [[vk::location(0)]] float4 color;
    [[vk::location(1)]] float2 texCooord;

    float4 hpos : SV_Position;
};


[shader("vertex")]
TEST_VS_OUTPUT main(uint vid : SV_VertexID)
{
    TEST_VS_OUTPUT result = (TEST_VS_OUTPUT)0;

    INPUT_DATA inputData = PrepareInputData(vid);

    result.texCooord = inputData.texCoord;
    result.color = inputData.color;
    result.hpos = float4(inputData.position, 0.5f, 1.f);

    return result;
}