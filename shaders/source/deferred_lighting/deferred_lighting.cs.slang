#include "common/common_defines.slang"

#include "registers/deferred_lighting_registers.slang"
#include "registers/common_registers.slang"

#include "common/lighting/brdf.slang"


float4 ComputeLighting(in uint2 pixelCoord)
{
    const int3 texCoord = int3(pixelCoord, 0);

    const float3 albedo = LOAD_TEX(DEFERRED_LIGHTING_GBUFFER_0, texCoord).rgb;
    const float3 normal = normalize(LOAD_TEX(DEFERRED_LIGHTING_GBUFFER_1, texCoord).xyz);
    const float3 metRoughAo = LOAD_TEX(DEFERRED_LIGHTING_GBUFFER_2, texCoord).rgb;

    const float2 uv = float2(pixelCoord) / float2(max(COMMON_SCREEN_SIZE, uint2(1, 1)));
    const float depth = LOAD_TEX(DEFERRED_LIGHTING_DEPTH, texCoord).x;
    
    const float3 wpos = WPosFromDepth(uv, depth, COMMON_CB.INV_VIEW_MATRIX, COMMON_CB.INV_PROJ_MATRIX);

    const float3 viewDir = normalize(wpos - COMMON_CAM_WPOS);

    return float4(albedo, 1.f);
}


[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint2 DTid : SV_DispatchThreadID)
{
    const uint2 pixelCoord = uint2(DTid.x, COMMON_SCREEN_SIZE.y - 1 - DTid.y);
    
    if (any(pixelCoord >= COMMON_SCREEN_SIZE)) {
        return;
    }

    DEFERRED_LIGHTING_OUTPUT_UAV[pixelCoord] = ComputeLighting(pixelCoord);
}