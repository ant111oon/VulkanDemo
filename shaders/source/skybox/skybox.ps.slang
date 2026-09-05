#include "registers/common_registers.slang"
#include "registers/skybox_registers.slang"

#include "pass/skybox/vsout.slang"


struct GPU_SkyboxPsOutput
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
GPU_SkyboxPsOutput main(GPU_SkyboxVsOutput input)
{
    GPU_SkyboxPsOutput result = (GPU_SkyboxPsOutput)0;

    result.color = SAMPLE_TEX_LEVEL(SKYBOX_TEXTURE, GET_COMMON_SMP(GPU_SamplerID::LINEAR_CLAMP_TO_EDGE), input.data.uvw, 0);

    return result;
}