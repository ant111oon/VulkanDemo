#include "common/common_cs.slang"

#include "common/math/distribution.slang"
#include "common/resource_defines.slang"

#include "common/lighting/brdf.slang"
#include "registers/brdf_integration_gen_registers.slang"


static const uint SAMPLES_NUM = 1024;


float2 IntegrateBRDF(in float NdotV, in float roughness)
{
    const float3 vView = float3(sqrt(1.f - NdotV * NdotV), 0.f, NdotV);

    float A = 0.f;
    float B = 0.f;

    const float3 vNorm = M3D_AXIS_Z;

    const float3 vUp = abs(vNorm.z) < 0.999f ? M3D_AXIS_Z : M3D_AXIS_X;
    const float3 vTang = normalize(cross(vUp, vNorm));
    const float3 vBitang = cross(vNorm, vTang);

    for (uint i = 0u; i < SAMPLES_NUM; ++i) {
        const float2 Xi = Hammersley(i, SAMPLES_NUM);
        const float3 vHalf = ImportanceSampleGGX(Xi, roughness, vTang, vBitang, vNorm);

        const float VdotH = saturate(dot(vView, vHalf));

        const float3 vLight = normalize(2.f * VdotH * vHalf - vView);

        float NdotL = max(vLight.z, 0.f);
        float NdotH = max(vHalf.z, 0.f);

        if (NdotL > 0.f) {
            const float k = CalcIndirectK(roughness);
            const float G = G_Smith(NdotV, NdotL, k);
        
            const float G_Vis = (G * VdotH) / (NdotH * NdotV);
            const float Fc = pow(1.f - VdotH, 5.f);

            A += (1.f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A /= float(SAMPLES_NUM);
    B /= float(SAMPLES_NUM);

    return float2(A, B);
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

    BRDF_INTEGRATION_GEN_OUTPUT_UAV[pixCoord] = float4(IntegrateBRDF(uv.x, uv.y), ZEROF2);
}