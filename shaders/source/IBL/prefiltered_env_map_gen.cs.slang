#include "common/common_cs.slang"

#include "common/math/distribution.slang"
#include "common/resource_defines.slang"

#include "common/lighting/brdf.slang"
#include "registers/prefiltered_env_map_gen_registers.slang"


static const uint SAMPLES_NUM = 1024;


float3 PreFilterEnvMap(in float roughness, in float3 R)
{
    float3 normal = normalize(R);
    float3 toEye = normal;

    SamplerState sampler = COMMON_SAMPLERS[(uint)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE];

    float3 res = ZEROF3;
    float totalWeight = 0.f;

    for (uint i = 0; i < SAMPLES_NUM; ++i) {
        float2 xi = Hammersley(i, SAMPLES_NUM);

        float3 H = ImportanceSampleGGX(xi, normal, roughness);
        float3 lightVec = 2.f * dot(toEye, H) * H - toEye;

        const float NdotL = saturate(dot(normal, lightVec));
        const float NdotH = saturate(dot(normal, H));
        const float HdotV = saturate(dot(H, toEye));

        if (NdotL > 0) {
            float D = NDF_GGX(roughness, NdotH);
            float pdf = (D * NdotH / (4.f * HdotV)) + M3D_EPS;

            float saTexel = 4.f * M3D_PI / (6.f * PUSH_CONSTS.ENV_MAP_FACE_SIZE.x * PUSH_CONSTS.ENV_MAP_FACE_SIZE.y);
            float saSample = 1.f / (SAMPLES_NUM * pdf + M3D_EPS);
             
            float mipLevel = M3D_IS_ZERO(roughness) ? 0.f : 0.5f * log2(saSample / saTexel);

            res += SAMPLE_TEX_LEVEL(PREFILTERED_ENV_MAP_GEN_ENV_MAP, sampler, lightVec, mipLevel).rgb * NdotL;     
            totalWeight += NdotL;
        }
    }

    return res / max(totalWeight, M3D_EPS);
}


[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (any(DTid >= uint3(PREFILTERED_ENV_MAP_OUTPUT_SIZE, 6))) {
        return;
    }

    const int2 pixCoord = DispatchThreadIDToPixCoord(DTid.xy, PREFILTERED_ENV_MAP_OUTPUT_SIZE);

    float2 uv = 2.f * PixelCoordToUV(pixCoord, PREFILTERED_ENV_MAP_OUTPUT_SIZE) - 1.f;
    const uint face = DTid.z;

    const float3 result = PreFilterEnvMap(PUSH_CONSTS.ROUGHNESS, UVFaceToDir(uv, face));

    PREFILTERED_ENV_MAP_GEN_OUTPUT_UAV[uint3(pixCoord, face)] = float4(result, 1.f);
}