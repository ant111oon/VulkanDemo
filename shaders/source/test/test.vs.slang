import "half_utils.fx";
import "registers_common.fx";

import "test_vsin.fx";
import "test_vsout.fx";

#ifdef ENV_USE_MESHLET_PIPELINE
    static const uint MESHLET_MAX_TRIANGLES = 128;
    static const uint MESHLET_MAX_VERTICES = 64;


    [shader("mesh")]
    [outputtopology("triangle")]
    [numthreads(MESHLET_MAX_TRIANGLES, 1, 1)]
    void main(uint gtid : SV_GroupThreadID, uint gid : SV_GroupID,
        out indices uint3 triangles[MESHLET_MAX_TRIANGLES],
        out vertices TEST_VS_OUTPUT vertices[MESHLET_MAX_VERTICES]
    ) {
        MESHLET meshlet = PrepareMeshletData(gid);
        SetMeshOutputCounts(meshlet.vertexCount, meshlet.triangleCount);

        if (gtid < meshlet.triangleCount) {
            triangles[gtid] = GetMehsletTriData(meshlet, gtid);
        }

        if (gtid < meshlet.vertexCount) {
            TEST_INPUT_DATA inputData = PrepareInputData(meshlet, gtid);

            vertices[gtid].lnorm    = inputData.lnorm;
            vertices[gtid].texCoord = inputData.texCoord;
            vertices[gtid].color    = float3(float(gid & 1), float(gid & 3) / 4, float(gid & 7) / 8);
            vertices[gtid].hpos     = mul(float4(inputData.position, 1.f), COMMON_CB.COMMON_VIEW_PROJ_MATRIX);
        }
    }
#else
    [shader("vertex")]
    TEST_VS_OUTPUT main(uint vid: SV_VertexID)
    {
        TEST_VS_OUTPUT result = (TEST_VS_OUTPUT)0;

        TEST_INPUT_DATA inputData = PrepareInputData(vid);

        result.lnorm = inputData.lnorm;
        result.texCoord = inputData.texCoord;

        result.hpos = mul(float4(inputData.position, 1.f), COMMON_CB.COMMON_VIEW_PROJ_MATRIX);

        return result;
    }
#endif