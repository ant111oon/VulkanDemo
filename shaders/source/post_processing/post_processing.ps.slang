#include "pass/post_processing/vsout.slang"

#include "registers/post_processing_registers.slang"
#include "registers/common_registers.slang"

#include "common/common_defines.slang"
#include "math/color_utils.slang"


struct POST_PROC_PS_OUTPUT
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
POST_PROC_PS_OUTPUT main(POST_PROC_VS_OUTPUT input, uint2 pixPos : SV_Position)
{
    POST_PROC_PS_OUTPUT result = (POST_PROC_PS_OUTPUT)0;

    result.color = LOAD_TEX(POST_PROCESSING_INPUT_COLOR, int3(pixPos, 0));

    return result;
}