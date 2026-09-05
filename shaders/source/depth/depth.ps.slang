#include "pass/depth/vsout.slang"

#include "registers/depth_registers.slang"

#include "common/gbuffer_incl.slang"


struct GPU_DepthPsOutput
{
};


[shader("fragment")]
GPU_DepthPsOutput main(GPU_DepthVsOutput input)
{
    GPU_DepthPsOutput result = (GPU_DepthPsOutput)0;

    if (DEPTH_IS_AKILL_PASS) {
        const GPU_GeomMaterial material = COMMON_MATERIALS[COMMON_INST_BUFFER[input.flatData.instID].materialID];
        const float alpha = SampleGBufferSurfaceAlbedo(material, GPU_CommonSamplerID::LINEAR_REPEAT, input.data.uv).a;

        if (alpha < material.alphaRef) {
            discard;
        }
    }

    return result;
}