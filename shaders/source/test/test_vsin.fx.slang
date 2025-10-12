__exported import "half_utils.fx";

#ifdef ENV_USE_MESHLET_PIPELINE
    struct MESHLET
    {
        uint vertexOffset;
        uint triangleOffset;
        uint vertexCount;
        uint triangleCount;
    };
#endif


struct TEST_BINDLESS_REGISTRY
{
    uint4* VERTEX_DATA;

#ifdef ENV_USE_MESHLET_PIPELINE
    MESHLET* MESHLET_DATA;
    uint*    MESHLET_VERT_IDX_DATA;
    uint*    MESHLET_TRI_DATA;
#endif
};

[[vk::push_constant]] TEST_BINDLESS_REGISTRY TEST_REGISTRY;


struct TEST_INPUT_DATA
{
    float3 position;
    float3 lnorm;
    float2 texCoord;
};

TEST_INPUT_DATA PrepareInputData(uint vertIdx)
{
    TEST_INPUT_DATA inputData = (TEST_INPUT_DATA)0;

    static const uint VERTEX_DATA_SIZE_UI4 = 1;
    const uint offset = vertIdx * VERTEX_DATA_SIZE_UI4;

    inputData.position.xy = UnpackHalf2x16(TEST_REGISTRY.VERTEX_DATA[offset][0]);
    inputData.position.z = UnpackHalf2x16(TEST_REGISTRY.VERTEX_DATA[offset][1]).x;

    inputData.lnorm.x = UnpackHalf2x16(TEST_REGISTRY.VERTEX_DATA[offset][1]).y;
    inputData.lnorm.yz = UnpackHalf2x16(TEST_REGISTRY.VERTEX_DATA[offset][2]);

    inputData.texCoord = UnpackHalf2x16(TEST_REGISTRY.VERTEX_DATA[offset][3]);

    return inputData;
}


#ifdef ENV_USE_MESHLET_PIPELINE
    MESHLET PrepareMeshletData(uint gid)
    {
        return TEST_REGISTRY.MESHLET_DATA[gid];
    }


    uint3 GetMehsletTriData(MESHLET meshlet, uint gtid)
    {
        const uint packed = TEST_REGISTRY.MESHLET_TRI_DATA[meshlet.triangleOffset + gtid];
        const uint vIdx0 = (packed >> 0) & 0xFF;
        const uint vIdx1 = (packed >> 8) & 0xFF;
        const uint vIdx2 = (packed >> 16) & 0xFF;
        
        return uint3(vIdx0, vIdx1, vIdx2);
    }


    TEST_INPUT_DATA PrepareInputData(MESHLET meshlet, uint gtid)
    {
        const uint vertIdx = TEST_REGISTRY.MESHLET_VERT_IDX_DATA[meshlet.vertexOffset + gtid];
        return PrepareInputData(vertIdx);
    }
#endif
