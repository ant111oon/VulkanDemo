__exported import "half_utils.fx";
__exported import "registers_test.fx";


struct TEST_INPUT_DATA
{
    float3 position;
    float3 lnorm;
    float2 texCoord;
};



TEST_INPUT_DATA PrepareInputData(uint vertIdx)
{
    TEST_INPUT_DATA inputData;

    static const uint VERTEX_DATA_SIZE_UI4 = 1;
    const uint offset = vertIdx * VERTEX_DATA_SIZE_UI4;

    inputData.position.xy = UnpackHalf2x16(TEST_REGISTRY.VERTEX_DATA[offset][0]);
    inputData.position.z  = UnpackHalf2x16(TEST_REGISTRY.VERTEX_DATA[offset][1]).x;

    inputData.lnorm.x  = UnpackHalf2x16(TEST_REGISTRY.VERTEX_DATA[offset][1]).y;
    inputData.lnorm.yz = UnpackHalf2x16(TEST_REGISTRY.VERTEX_DATA[offset][2]);

    inputData.texCoord = UnpackHalf2x16(TEST_REGISTRY.VERTEX_DATA[offset][3]);

    return inputData;
}