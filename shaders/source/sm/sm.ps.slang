#include "pass/sm/vsout.slang"

#include "registers/sm_registers.slang"

#include "common/gbuffer_incl.slang"


struct SM_PS_OUTPUT
{
};


[shader("fragment")]
SM_PS_OUTPUT main(SM_VS_OUTPUT input)
{
    SM_PS_OUTPUT result = (SM_PS_OUTPUT)0;

    if (CSM_IS_AKILL_PASS) {
        const COMMON_MATERIAL material = COMMON_MATERIALS[COMMON_INST_BUFFER[input.flatData.instID].MATERIAL_IDX];
        const float alpha = SampleGBufferSurfaceAlbedo(material, COMMON_SAMPLER_IDX::LINEAR_REPEAT, input.data.uv).a;

        if (alpha < material.ALPHA_REF) {
            discard;
        }
    }

    return result;
}