struct TEST_VS_OUTPUT_INTERP
{
    [[vk::location(0)]] float3 lnorm;
    [[vk::location(1)]] float2 texCoord;
};


struct TEST_VS_OUTPUT_NO_INTERP
{
    [[vk::location(2)]] uint instInfoID;
};


struct TEST_VS_OUTPUT
{
    TEST_VS_OUTPUT_INTERP data;
    nointerpolation TEST_VS_OUTPUT_NO_INTERP flatData;

    float4 hpos : SV_Position; 
};