#include "registers/irradiance_map_gen_registers.slang"

#include "common/math/math.slang"
#include "common/common_defines.slang"

#include "common/common_cs.slang"


static const uint IRRADIANCE_MAP_GEN_SAMPLES_NUM = 2048;


// Mirror binary digits about the decimal point
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}


// Randomish sequence that has pretty evenly spaced points
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}


float3 CosineSampleHemisphere(in float2 Xi)
{
    const float phi = M3D_2_PI * Xi.x;
    const float cosTheta = 1.f - 2.f * Xi.y;
    const float sinTheta = sqrt(1.f - cosTheta * cosTheta);

    return normalize(float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta));
}


[shader("compute")]
[numthreads(IRRADIANCE_MAP_GEN_GROUP_THREAD_X, IRRADIANCE_MAP_GEN_GROUP_THREAD_Y, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (any(DTid >= uint3(IRRADIANCE_MAP_GEN_OUTPUT_SIZE, 6))) {
        return;
    }

    const int2 pixCoord = DispatchThreadIDToPixCoord(DTid.xy, IRRADIANCE_MAP_GEN_OUTPUT_SIZE);

    float2 uv = 2.f * PixelCoordToUV(pixCoord, IRRADIANCE_MAP_GEN_OUTPUT_SIZE) - 1.f;
    const uint face = DTid.z;

    const float3 normal = UVFaceToDir(uv, face);

    float3 irradiance = ZEROF3;

    SamplerState sampler = COMMON_SAMPLERS[(uint)COMMON_SAMPLER_IDX::LINEAR_CLAMP_TO_EDGE];

    for (uint i = 0; i < IRRADIANCE_MAP_GEN_SAMPLES_NUM; ++i) {
        const float2 Xi = Hammersley(i, IRRADIANCE_MAP_GEN_SAMPLES_NUM);
        const float3 L = CosineSampleHemisphere(Xi);

        const float NdotL = max(dot(normal, L), 0.f);
        irradiance += SAMPLE_TEX_LEVEL(IRRADIANCE_MAP_GEN_ENV_MAP, sampler, L, 0).rgb * NdotL;
    }

    irradiance *= (4.f * M3D_PI / IRRADIANCE_MAP_GEN_SAMPLES_NUM);

    IRRADIANCE_MAP_GEN_OUTPUT_UAV[uint3(pixCoord, face)] = float4(irradiance, 1.f);
}