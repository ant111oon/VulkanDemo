struct TEST_VS_OUTPUT
{
    [[vk::location(0)]] float3 lnorm;
    [[vk::location(1)]] float2 texCoord;

#ifdef ENV_USE_MESHLET_PIPELINE
    [[vk::location(2)]] float3 color;
#endif

    float4 hpos : SV_Position; 
};