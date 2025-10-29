struct TEST_BINDLESS_REGISTRY
{
    uint4 *VERTEX_DATA;

    uint DRAW_MTL_IDX;
    uint DBG_TEX_IDX;
};

[[vk::push_constant]] TEST_BINDLESS_REGISTRY TEST_REGISTRY;