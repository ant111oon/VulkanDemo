struct TEST_BINDLESS_REGISTRY
{
    uint4 *VERTEX_DATA;

    uint TEX_IDX;
    uint PADD;
};

[[vk::push_constant]] TEST_BINDLESS_REGISTRY TEST_REGISTRY;