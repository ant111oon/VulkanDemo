#include "common/common_cs.slang"

#include "common/math/distribution.slang"
#include "common/resource_defines.slang"

#include "common/lighting/brdf.slang"
#include "registers/prefiltered_env_map_gen_registers.slang"


static const uint SAMPLES_NUM = 32;


float3 PreFilterEnvMap(in float roughness, in float3 R)
{
    const float3 vNorm = normalize(R);
    const float3 vView = vNorm;

    const float3 vUp = abs(vNorm.z) < 0.999f ? M3D_AXIS_Z : M3D_AXIS_X;
    const float3 vTang = normalize(cross(vUp, vNorm));
    const float3 vBitang = cross(vNorm, vTang);

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
            const float saTexel = 4.f * M3D_PI / max(6.f * PUSH_CONSTS.ENV_MAP_FACE_SIZE.x * PUSH_CONSTS.ENV_MAP_FACE_SIZE.y, M3D_EPS);
            // Solid angle represented by this sample
            const float saSample = 1.f / max(SAMPLES_NUM * pdf, M3D_EPS);

            const float mipBias = 1.f;
            const float mipLevel = M3D_IS_ZERO(roughness) ? 0.f : max(0.5f * log2(saSample / saTexel) + mipBias, 0.f);

            result += SAMPLE_TEX_LEVEL(PREFILTERED_ENV_MAP_GEN_ENV_MAP, GET_COMMON_SAMPLER(LINEAR_CLAMP_TO_EDGE), vLight, mipLevel).rgb * NdotL; 
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
    outputSize.xy = COMMON_PREFILTERED_ENV_MAP_SIZE >> PUSH_CONSTS.MIP;

    if (any(DTid.xy >= outputSize)) {
        return;
    }

    const int2 pixCoord = DispatchThreadIDToPixCoord(DTid.xy, outputSize.xy);

    float2 uv = 2.f * PixelCoordToUV(pixCoord, outputSize.xy) - 1.f;
    const uint face = DTid.z;

    const float roughness = saturate(COMMON_PREFILTERED_ENV_MAP_MIP_ROUGHNESS_DELTA * PUSH_CONSTS.MIP);

    const float3 result = PreFilterEnvMap(roughness, UVFaceToDir(uv, face));

    PREFILTERED_ENV_MAP_GEN_OUTPUT_UAV[uint3(pixCoord, face)] = float4(result, 1.f);
}