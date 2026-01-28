#include "common/common_cs.slang"

#include "common/math/distribution.slang"
#include "common/resource_defines.slang"

#include "common/lighting/brdf.slang"
#include "registers/brdf_integration_gen_registers.slang"


static const uint SAMPLES_NUM = 1024;


float2 IntegrateBRDF(in float NdotV, in float roughness)
{
    const float3 V = float3(sqrt(1.f - NdotV * NdotV), 0.f, NdotV);

    float A = 0.f;
    float B = 0.f;

    float3 N = M3D_AXIS_Z;

    for (uint i = 0u; i < SAMPLES_NUM; ++i) {
        const float2 Xi = Hammersley(i, SAMPLES_NUM);
        const float3 H = ImportanceSampleGGX(Xi, N, roughness);

        const float VdotH = saturate(dot(V, H));

        const float3 L = normalize(2.f * VdotH * H - V);

        float NdotL = max(L.z, 0.f);
        float NdotH = max(H.z, 0.f);

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
    if (any(DTid.xy >= BRDF_INTEGRATION_OUTPUT_SIZE)) {
        return;
    }

    const int2 pixCoord = DispatchThreadIDToPixCoord(DTid.xy, BRDF_INTEGRATION_OUTPUT_SIZE);

    float2 uv = PixelCoordToUV(pixCoord, BRDF_INTEGRATION_OUTPUT_SIZE);

    BRDF_INTEGRATION_GEN_OUTPUT_UAV[pixCoord] = float4(IntegrateBRDF(uv.x, uv.y), ZEROF2);
}