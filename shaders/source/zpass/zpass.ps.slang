#include "pass/zpass/vsout.slang"

#include "common/common_structs.slang"


struct ZPASS_PS_OUTPUT
{
};


[shader("fragment")]
ZPASS_PS_OUTPUT main(ZPASS_VS_OUTPUT input)
{
    ZPASS_PS_OUTPUT result = (ZPASS_PS_OUTPUT)0;

    // Move this code under define for example COMMON_AKILL or to separate shader
    const COMMON_INST_INFO instInfo = COMMON_INST_INFOS[input.flatData.instID];
    const COMMON_MATERIAL material = COMMON_MATERIALS[instInfo.MATERIAL_IDX];

    if (IS_COMMON_MATERIAL_ALPHA_KILL(material)) {
        const float alpha = SampleCommonSurfaceAlbedo(material, (uint)COMMON_SAMPLER_IDX::LINEAR_REPEAT, input.data.uv).a;

        if (alpha < material.ALPHA_REF) {
            discard;
        }
    }

    return result;
}