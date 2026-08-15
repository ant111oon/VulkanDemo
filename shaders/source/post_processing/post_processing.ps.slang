#include "registers/common_registers.slang"
#include "registers/post_processing_registers.slang"

#include "common/tonemapping.slang"

#include "pass/post_processing/vsout.slang"


float3 Tonemap(in float3 color)
{
    switch (COMMON_DBG_TONEMAP_PRESET) {
        case GPU_DbgTonemapPreset::ACES:                return ACES_Tonemap(color);
        case GPU_DbgTonemapPreset::REINHARD:            return Reinhard_Tonemap(color);
        case GPU_DbgTonemapPreset::PARTIAL_UNCHARTED_2: return PartialUncharted2_Tonemap(color);
        case GPU_DbgTonemapPreset::UNCHARTED_2:         return Uncharted2_Tonemap(color);
        default:                                        return float3(1.f, 0.f, 0.f);
    }
}


struct GPU_PostProcPsOutput
{
    [[vk::location(0)]] float4 color;
};


[shader("fragment")]
GPU_PostProcPsOutput main(GPU_PostProcVsOutput input)
{
    float3 color = LOAD_TEX(POST_PROCESSING_INPUT_COLOR, int3((int2)input.hpos.xy, 0)).rgb;
    color = Tonemap(color);

    GPU_PostProcPsOutput result = (GPU_PostProcPsOutput)0;

    result.color = float4(color, 1.f);

    return result;
}