#include "registers/common_registers.slang"
#include "registers/prefiltered_env_map_gen_registers.slang"

#include "common/math/distribution.slang"

#include "common/common_cs.slang"

#include "common/brdf.slang"


static const uint SAMPLES_NUM = 32;


float3 PreFilterEnvMap(in float roughness, in float3 vReflect)
{
    const float3 vNorm = normalize(vReflect);
    const float3 vView = vNorm;

    float3 vTang, vBitang;
    CalcTangBitang(vNorm, vTang, vBitang);

    float3 result = ZEROF3;
    float totalWeight = 0.f;

    for (uint i = 0; i < SAMPLES_NUM; ++i) {
        const float2 vXi = Hammersley(i, SAMPLES_NUM);

        const float3 vHalf = ImportanceSampleGGX(vXi, roughness, vTang, vBitang, vNorm);
        const float VdotH = saturate(dot(vView, vHalf));

        const float3 vLight = normalize(2.f * VdotH * vHalf - vView);

        const float NdotL = saturate(dot(vNorm, vLight));

        if (NdotL > 0) {
            const float NdotH = saturate(dot(vNorm, vHalf));

            const float pdf = (NDF_GGX(roughness, NdotH) * NdotH / (4.f * VdotH));

            // Solid angle covered by 1 pixel with 6 faces that are EnvMapSize X EnvMapSize
            const float saTexel = 4.f * M3D_PI / max(6.f * PREFILTERED_ENV_MAP_FACE_SIZE.x * PREFILTERED_ENV_MAP_FACE_SIZE.y, M3D_EPS);
            // Solid angle represented by this sample
            const float saSample = 1.f / max(SAMPLES_NUM * pdf, M3D_EPS);

            const float mipBias = 1.f;
            const float mipLevel = M3D_IS_ZERO(roughness) ? 0.f : max(0.5f * log2(saSample / saTexel) + mipBias, 0.f);

            result += SAMPLE_TEX_LEVEL(PREFILTERED_ENV_MAP_GEN_ENV_MAP, GET_COMMON_SAMPLER(COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE), vLight, mipLevel).rgb * NdotL; 
            totalWeight += NdotL;
        }
    }

    return result / max(totalWeight, M3D_EPS);
}


[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (PREFILTERED_ENV_MAP_MIP >= COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT) {
        return;
    }

    uint2 outputSize = ZEROU2;
    outputSize.xy = COMMON_PREFILTERED_ENV_MAP_SIZE >> PREFILTERED_ENV_MAP_MIP;

    if (any(DTid.xy >= outputSize)) {
        return;
    }

    const int2 pixCoord = DispatchThreadIDToPixCoord(DTid.xy, outputSize.xy);

    float2 uv = 2.f * PixelCoordToUV(pixCoord, outputSize.xy) - 1.f;
    const uint face = DTid.z;

    const float roughness = saturate(COMMON_PREFILTERED_ENV_MAP_MIP_ROUGHNESS_DELTA * PREFILTERED_ENV_MAP_MIP);

    const uint dstIdx = face * COMMON_PREFILTERED_ENV_MAP_MIPS_COUNT + PREFILTERED_ENV_MAP_MIP;
    PREFILTERED_ENV_MAP_GEN_OUTPUT_UAV[dstIdx][pixCoord] = float4(PreFilterEnvMap(roughness, UVFaceToDir(uv, face)), 1.f);
}