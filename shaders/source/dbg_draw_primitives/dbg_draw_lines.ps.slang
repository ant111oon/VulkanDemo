#define DBG_DRAW_LINES_PASS

#include "registers/dbg_draw_lines_registers.slang"

#include "pass/dbg_draw_primitives/vsout.slang"


struct GPU_DbgDrawPsOutput
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
GPU_DbgDrawPsOutput main(GPU_DbgDrawVsOutput input)
{
    GPU_DbgDrawPsOutput result = (GPU_DbgDrawPsOutput)0;
    result.color = input.data.color;

    return result;
}