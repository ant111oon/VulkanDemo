#include "registers/irradiance_map_gen_registers.slang"

#include "common/math/distribution.slang"
#include "common/resource_defines.slang"

#include "common/common_cs.slang"


static const uint IRRADIANCE_MAP_GEN_SAMPLES_NUM = 2048;


float3 CosineSampleHemisphere(in float2 Xi)
{
    const float phi = M3D_2_PI * Xi.x;
    const float cosTheta = 1.f - 2.f * Xi.y;
    const float sinTheta = sqrt(1.f - cosTheta * cosTheta);

    float sinPhi, cosPhi;
    sincos(phi, sinPhi, cosPhi);

    return normalize(float3(cosPhi * sinTheta, sinPhi * sinTheta, cosTheta));
}


float3 CalcIrradiance(in float3 normal)
{    
    float3 irradiance = ZEROF3;

    for (uint i = 0; i < IRRADIANCE_MAP_GEN_SAMPLES_NUM; ++i) {
        const float2 Xi = Hammersley(i, IRRADIANCE_MAP_GEN_SAMPLES_NUM);
        const float3 L = CosineSampleHemisphere(Xi);

        const float NdotL = max(dot(normal, L), 0.f);
        irradiance += SAMPLE_TEX_LEVEL(IRRADIANCE_MAP_GEN_ENV_MAP, GET_COMMON_SAMPLER(LINEAR_CLAMP_TO_EDGE), L, 0).rgb * NdotL;
    }

    return irradiance * (4.f * M3D_PI / IRRADIANCE_MAP_GEN_SAMPLES_NUM);
}


[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (any(DTid >= uint3(COMMON_IRRADIANCE_MAP_SIZE, 6))) {
        return;
    }

    const int2 pixCoord = DispatchThreadIDToPixCoord(DTid.xy, COMMON_IRRADIANCE_MAP_SIZE);

    float2 uv = 2.f * PixelCoordToUV(pixCoord, COMMON_IRRADIANCE_MAP_SIZE) - 1.f;
    const uint face = DTid.z;

    const float3 normal = UVFaceToDir(uv, face);

    IRRADIANCE_MAP_GEN_OUTPUT_UAV[uint3(pixCoord, face)] = float4(CalcIrradiance(normal), 1.f);
}