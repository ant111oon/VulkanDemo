#include "common/common_cs.slang"

#include "common/math/distribution.slang"
#include "common/resource_defines.slang"

#include "common/lighting/brdf.slang"
#include "registers/brdf_integration_gen_registers.slang"


static const uint SAMPLES_NUM = 1024;


float2 IntegrateBRDF(in float NdotV, in float roughness)
{
    const float3 vView = float3(sqrt(1.f - NdotV * NdotV), 0.f, NdotV);

    const float3 vNorm = M3D_AXIS_Z;

    float3 vTang, vBitang;
    CalcTangBitang(vNorm, vTang, vBitang);

    float2 result = ZEROF2;

    for (uint i = 0u; i < SAMPLES_NUM; ++i) {
        const float2 Xi = Hammersley(i, SAMPLES_NUM);
        const float3 vHalf = ImportanceSampleGGX(Xi, roughness, vTang, vBitang, vNorm);

        const float VdotH = saturate(dot(vView, vHalf));

        const float3 vLight = normalize(2.f * VdotH * vHalf - vView);

        float NdotL = max(vLight.z, 0.f);
        float NdotH = max(vHalf.z, M3D_EPS);

        if (NdotL > 0.f) {
            const float k = CalcIndirectK(roughness);
            const float G = G_Smith(NdotV, NdotL, k);

            const float G_Vis = (G * VdotH) / max(NdotH * NdotV, M3D_EPS);
            const float Fc = pow(1.f - VdotH, 5.f);

            result.x += (1.f - Fc) * G_Vis;
            result.y += Fc * G_Vis;
        }
    }

    return result / SAMPLES_NUM;
}


[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (any(DTid.xy >= COMMON_BRDF_INTEGRATION_LUT_SIZE)) {
        return;
    }

    const int2 pixCoord = DispatchThreadIDToPixCoord(DTid.xy, COMMON_BRDF_INTEGRATION_LUT_SIZE);

    float2 uv = PixelCoordToUV(pixCoord, COMMON_BRDF_INTEGRATION_LUT_SIZE);

    BRDF_INTEGRATION_GEN_OUTPUT_UAV[pixCoord] = float4(IntegrateBRDF(uv.x, uv.y), 0.f, 0.f);
}