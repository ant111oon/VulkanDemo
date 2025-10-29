import "test_vsout.fx";
import "registers_common.fx";

import "registers_test.fx";


struct TEST_PS_OUTPUT
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
TEST_PS_OUTPUT main(TEST_VS_OUTPUT input)
{
    TEST_PS_OUTPUT result = (TEST_PS_OUTPUT)0;

    const COMMON_MATERIAL material = COMMON_MATERIALS[TEST_REGISTRY.DRAW_MTL_IDX];

    const int texIndices[5] = { 
        material.albedoTexIdx, 
        material.normalTexIdx, 
        material.metallicRoughnessTexIdx, 
        material.aoTexIdx, 
        material.emissiveTexIdx
    };

    const int texIdx = texIndices[TEST_REGISTRY.DBG_TEX_IDX];

    result.color = float4(0.f, 0.f, 0.f, 1.f);

    if (texIdx != -1) {
        result.color = COMMON_MTL_TEXTURES[texIdx].Sample(COMMON_SAMPLER, input.texCoord);
    }

    if (result.color.a <= 0.0001f) {
        discard;
    }
    
    return result;
}