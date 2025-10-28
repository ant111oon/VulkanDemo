struct COMMON_CB_DATA
{
    float4x4 COMMON_VIEW_MATRIX;
    float4x4 COMMON_PROJ_MATRIX;
    float4x4 COMMON_VIEW_PROJ_MATRIX;
}; 


[vk::binding(0, 0)] ConstantBuffer<COMMON_CB_DATA> COMMON_CB;

[vk::binding(1, 0)] SamplerState COMMON_SAMPLER;

[vk::binding(2, 0)] Texture2D COMMON_MTL_TEXTURES[128];

