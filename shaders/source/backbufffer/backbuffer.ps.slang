#include "pass/backbuffer/vsout.slang"

#include "registers/common_registers.slang"
#include "registers/backbuffer_registers.slang"

#include "common/resource_defines.slang"


struct BACKBUFFER_PS_OUTPUT
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
BACKBUFFER_PS_OUTPUT main(BACKBUFFER_VS_OUTPUT input)
{
    BACKBUFFER_PS_OUTPUT result = (BACKBUFFER_PS_OUTPUT)0;

    result.color = float4(LOAD_TEX(BACKBUFFER_INPUT_COLOR, int3((int2)input.hpos.xy, 0)).rgb, 1.f);

    return result;
}