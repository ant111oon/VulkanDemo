#include "pass/csm/vsout.slang"

#include "registers/csm_registers.slang"

#include "common/gbuffer_incl.slang"


struct GPU_CsmPsOutput
{
};


[shader("fragment")]
GPU_CsmPsOutput main(GPU_CsmVsOutput input)
{
    GPU_CsmPsOutput result = (GPU_CsmPsOutput)0;

    if (CSM_IS_AKILL_PASS) {
        const GPU_GeomMaterial material = COMMON_MATERIALS[COMMON_INST_BUFFER[input.flatData.instID].materialID];
        const float alpha = SampleGBufferSurfaceAlbedo(material, GPU_CommonSamplerID::LINEAR_REPEAT, input.data.uv).a;

        if (alpha < material.alphaRef) {
            discard;
        }
    }

    return result;
}