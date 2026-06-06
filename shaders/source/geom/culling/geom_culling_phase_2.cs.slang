#include "common/geom_culling_incl.slang"


[numthreads(GEOM_CULLING_CS_GROUP_SIZE, 1, 1)]
[shader("compute")]
void main(uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
    ExecuteGeomCulling<GEOM_CULLING_PHASE::PHASE_2>(DTid, GI);
}