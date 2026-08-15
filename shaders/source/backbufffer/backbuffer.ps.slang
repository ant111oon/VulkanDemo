#include "pass/backbuffer/vsout.slang"

#include "registers/common_registers.slang"
#include "registers/backbuffer_registers.slang"

#include "common/system/sys_constants.slang"


struct GPU_BackbufferPsOutput
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
GPU_BackbufferPsOutput main(GPU_BackbufferVsOutput input)
{
    GPU_BackbufferPsOutput result = (GPU_BackbufferPsOutput)0;

    result.color = float4(LOAD_TEX(BACKBUFFER_INPUT_COLOR, int3((int2)input.hpos.xy, 0)).rgb, 1.f);

    return result;
}