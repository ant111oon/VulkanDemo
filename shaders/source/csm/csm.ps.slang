#include "pass/csm/vsout.slang"

#include "registers/csm_registers.slang"

#include "common/gbuffer_incl.slang"


struct CSM_PS_OUTPUT
{
};


[shader("fragment")]
CSM_PS_OUTPUT main(CSM_VS_OUTPUT input)
{
    CSM_PS_OUTPUT result = (CSM_PS_OUTPUT)0;

    if (CSM_IS_AKILL_PASS) {
        const GPU_GeomMaterial material = COMMON_MATERIALS[COMMON_INST_BUFFER[input.flatData.instID].materialID];
        const float alpha = SampleGBufferSurfaceAlbedo(material, GPU_CommonSamplerID::LINEAR_REPEAT, input.data.uv).a;

        if (alpha < material.alphaRef) {
            discard;
        }
    }

    return result;
}