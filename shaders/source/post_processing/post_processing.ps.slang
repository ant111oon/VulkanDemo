#include "registers/common_registers.slang"
#include "registers/post_processing_registers.slang"

#include "common/tonemapping.slang"

#include "pass/post_processing/vsout.slang"


float3 Tonemap(in float3 color)
{
    switch (COMMON_DBG_TONEMAP_PRESET) {
        case DBG_TONEMAP_PRESET::ACES:                  return ACES_Tonemap(color);
        case DBG_TONEMAP_PRESET::REINHARD:              return Reinhard_Tonemap(color);
        case DBG_TONEMAP_PRESET::PARTIAL_UNCHARTED_2:   return PartialUncharted2_Tonemap(color);
        case DBG_TONEMAP_PRESET::UNCHARTED_2:           return Uncharted2_Tonemap(color);
        default:                                        return float3(1.f, 0.f, 0.f);
    }
}


struct POST_PROC_PS_OUTPUT
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
POST_PROC_PS_OUTPUT main(POST_PROC_VS_OUTPUT input)
{
    float3 color = LOAD_TEX(POST_PROCESSING_INPUT_COLOR, int3((int2)input.hpos.xy, 0)).rgb;
    color = Tonemap(color);

    POST_PROC_PS_OUTPUT result = (POST_PROC_PS_OUTPUT)0;

    result.color = float4(color, 1.f);

    return result;
}