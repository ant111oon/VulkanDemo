struct TEST_VS_OUTPUT
{
    [[vk::location(0)]] float3 lnorm;
    [[vk::location(1)]] float2 texCoord;

    float4 hpos : SV_Position; 
};