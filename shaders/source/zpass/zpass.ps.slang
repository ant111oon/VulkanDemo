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
        const GPU_GeomMaterial material = COMMON_MATERIALS[COMMON_INST_BUFFER[input.flatData.instID].materialID];
        const float alpha = SampleGBufferSurfaceAlbedo(material, GPU_CommonSamplerID::LINEAR_REPEAT, input.data.uv).a;

        if (alpha < material.alphaRef) {
            discard;
        }
    }

    return result;
}