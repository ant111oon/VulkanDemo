struct COMMON_CB_DATA
{
    float4x4 COMMON_VIEW_MATRIX;
    float4x4 COMMON_PROJ_MATRIX;
    float4x4 COMMON_VIEW_PROJ_MATRIX;
}; 


[vk::binding(0, 0)] ConstantBuffer<COMMON_CB_DATA> COMMON_CB;