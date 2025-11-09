struct TEST_BINDLESS_REGISTRY
{
    uint4* VERTEX_DATA;

    uint INST_INFO_IDX;
    uint PADDING;
};


[[vk::push_constant]] TEST_BINDLESS_REGISTRY TEST_REGISTRY;