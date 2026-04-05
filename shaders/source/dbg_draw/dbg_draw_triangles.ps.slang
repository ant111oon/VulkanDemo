#define DBG_DRAW_TRIANGLES_PASS

#include "registers/dbg_draw_registers.slang"

#include "pass/dbg_draw/vsout.slang"


struct DBG_DRAW_PS_OUTPUT
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
DBG_DRAW_PS_OUTPUT main(DBG_DRAW_VS_OUTPUT input)
{
    DBG_DRAW_PS_OUTPUT result = (DBG_DRAW_PS_OUTPUT)0;
    result.color = input.data.color;

    return result;
}