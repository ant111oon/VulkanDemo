#include "pass/zpass/vsout.slang"

#include "registers/zpass_registers.slang"

#include "common/gbuffer_incl.slang"


struct ZPASS_PS_OUTPUT
{
};


[shader("fragment")]
ZPASS_PS_OUTPUT main(ZPASS_VS_OUTPUT input)
{
    ZPASS_PS_OUTPUT result = (ZPASS_PS_OUTPUT)0;

    if (ZPASS_IS_AKILL_PASS) {
        const COMMON_MATERIAL material = COMMON_MATERIALS[COMMON_INST_BUFFER[input.flatData.instID].MATERIAL_IDX];
        const float alpha = SampleGBufferSurfaceAlbedo(material, COMMON_SAMPLER_IDX::LINEAR_REPEAT, input.data.uv).a;

        if (alpha < material.ALPHA_REF) {
            discard;
        }
    }

    return result;
}