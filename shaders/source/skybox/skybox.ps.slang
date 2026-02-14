#include "pass/skybox/vsout.slang"
#include "registers/skybox_registers.slang"

#include "common/resource_defines.slang"


struct SKYBOX_PS_OUTPUT
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
SKYBOX_PS_OUTPUT main(SKYBOX_VS_OUTPUT input)
{
    SKYBOX_PS_OUTPUT result = (SKYBOX_PS_OUTPUT)0;

    result.color = SAMPLE_TEX_LEVEL(SKYBOX_TEXTURE, GET_COMMON_SAMPLER(COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE), input.data.uvw, 0);

    return result;
}