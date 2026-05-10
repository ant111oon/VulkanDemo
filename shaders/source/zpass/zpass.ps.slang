#include "pass/zpass/vsout.slang"

#include "registers/common_registers.slang"

#include "common/gbuffer_incl.slang"


struct ZPASS_PS_OUTPUT
{
};


[shader("fragment")]
ZPASS_PS_OUTPUT main(ZPASS_VS_OUTPUT input)
{
    ZPASS_PS_OUTPUT result = (ZPASS_PS_OUTPUT)0;

    // Move this code under define for example COMMON_AKILL or to separate shader
    const COMMON_INST inst = COMMON_INST_BUFFER[input.flatData.instID];
    const COMMON_MATERIAL material = COMMON_MATERIALS[inst.MATERIAL_IDX];

    if (material.IsAlphaKill()) {
        const float alpha = SampleGBufferSurfaceAlbedo(material, (uint)COMMON_SAMPLER_IDX::ANISO_4X_LINEAR_REPEAT, input.data.uv).a;

        if (alpha < material.ALPHA_REF) {
            discard;
        }
    }

    return result;
}