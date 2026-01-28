#include "common/common_cs.slang"

#include "common/math/distribution.slang"
#include "common/resource_defines.slang"

#include "common/lighting/brdf.slang"
#include "registers/prefiltered_env_map_gen_registers.slang"


static const uint SAMPLES_NUM = 1024;


float3 PreFilterEnvMap(in float roughness, in float3 R)
{
    float3 N = normalize(R);
    float3 V = N;

    SamplerState sampler = COMMON_SAMPLERS[(uint)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE];

    float3 result = ZEROF3;
    float totalWeight = 0.f;

    for (uint i = 0; i < SAMPLES_NUM; ++i) {
        float2 xi = Hammersley(i, SAMPLES_NUM);

        float3 H = ImportanceSampleGGX(xi, N, roughness);

        const float HdotV = saturate(dot(H, V));

        float3 lightVec = normalize(2.f * HdotV * H - V);

        const float NdotL = saturate(dot(N, lightVec));
        const float NdotH = saturate(dot(N, H));

        if (NdotL > 0) {
            float D = NDF_GGX(roughness, NdotH);
            float pdf = (D * NdotH / (4.f * HdotV)) + M3D_EPS;

            float saTexel = 4.f * M3D_PI / (6.f * PUSH_CONSTS.ENV_MAP_FACE_SIZE.x * PUSH_CONSTS.ENV_MAP_FACE_SIZE.y);
            float saSample = 1.f / (SAMPLES_NUM * pdf + M3D_EPS);
             
            float mipLevel = M3D_IS_ZERO(roughness) ? 0.f : 0.5f * log2(saSample / saTexel);

            result += SAMPLE_TEX_LEVEL(PREFILTERED_ENV_MAP_GEN_ENV_MAP, sampler, lightVec, mipLevel).rgb * NdotL; 
            totalWeight += NdotL;
        }
    }

    return result / max(totalWeight, M3D_EPS);
}


[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (PUSH_CONSTS.MIP >= COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT) {
        return;
    }

    uint2 outputSize = ZEROU2;
    outputSize.xy = PREFILTERED_ENV_MAP_OUTPUT_SIZE >> PUSH_CONSTS.MIP;

    if (any(DTid.xy >= outputSize)) {
        return;
    }

    const int2 pixCoord = DispatchThreadIDToPixCoord(DTid.xy, outputSize.xy);

    float2 uv = 2.f * PixelCoordToUV(pixCoord, outputSize.xy) - 1.f;
    const uint face = DTid.z;

    const float roughness = saturate(PREFILTERED_ENV_MAP_MIP_ROUGHNESS_DELTA * PUSH_CONSTS.MIP);

    const float3 result = PreFilterEnvMap(roughness, UVFaceToDir(uv, face));

    PREFILTERED_ENV_MAP_GEN_OUTPUT_UAV[uint3(pixCoord, face)] = float4(result, 1.f);
}