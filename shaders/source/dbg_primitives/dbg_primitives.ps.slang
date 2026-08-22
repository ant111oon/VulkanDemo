#define DBG_DRAW_TRIANGLES_PASS

#include "registers/dbg_primitives_registers.slang"

#include "pass/dbg_primitives/vsout.slang"


struct GPU_DbgDrawPsOutput
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
GPU_DbgDrawPsOutput main(GPU_DbgPrimVsOutput input)
{
    GPU_DbgDrawPsOutput result = (GPU_DbgDrawPsOutput)0;
    result.color = input.data.color;

    return result;
}