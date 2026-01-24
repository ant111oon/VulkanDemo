#include "pass/post_processing/vsout.slang"

#include "registers/post_processing_registers.slang"
#include "registers/common_registers.slang"

#include "common/common_defines.slang"
#include "common/tonemapping.slang"
#include "common/math/utils.slang"


float3 Tonemap(in float3 color)
{
#ifdef ENV_DEBUG
    if (!COMMON_DBG_VIS_NONE) {
        return color;
    }
#endif

#if defined(ENV_RELEASE)
    return ACES_Tonemap(color);
#else
    if (COMMON_DBG_USE_ACES_TONEMAPPING) {
        return ACES_Tonemap(color);
    } else if (COMMON_DBG_USE_REINHARD_TONEMAPPING) {
        return Reinhard_Tonemap(color);
    } else if (COMMON_DBG_USE_PARTIAL_UNCHARTED_2_TONEMAPPING) {
        return PartialUncharted2_Tonemap(color);
    } else if (COMMON_DBG_USE_UNCHARTED_2_TONEMAPPING) {
        return Uncharted2_Tonemap(color);
    } else {
        return float3(1.f, 0.f, 0.f); // Invalid tonemapping flag
    }
#endif
}


struct POST_PROC_PS_OUTPUT
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
POST_PROC_PS_OUTPUT main(POST_PROC_VS_OUTPUT input, uint2 pixPos : SV_Position)
{
    float3 color = LOAD_TEX(POST_PROCESSING_INPUT_COLOR, int3(pixPos, 0)).rgb;
    color = Tonemap(color);

    POST_PROC_PS_OUTPUT result = (POST_PROC_PS_OUTPUT)0;

    result.color = float4(color, 1.f);

    return result;
}