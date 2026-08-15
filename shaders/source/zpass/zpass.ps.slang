#include "pass/zpass/vsout.slang"

#include "registers/zpass_registers.slang"

#include "common/gbuffer_incl.slang"


struct GPU_ZPassPsOutput
{
};


[shader("fragment")]
GPU_ZPassPsOutput main(GPU_ZPassVsOutput input)
{
    GPU_ZPassPsOutput result = (GPU_ZPassPsOutput)0;

    if (ZPASS_IS_AKILL_PASS) {
        const GPU_GeomMaterial material = COMMON_MATERIALS[COMMON_INST_BUFFER[input.flatData.instID].materialID];
        const float alpha = SampleGBufferSurfaceAlbedo(material, GPU_CommonSamplerID::LINEAR_REPEAT, input.data.uv).a;

        if (alpha < material.alphaRef) {
            discard;
        }
    }

    return result;
}