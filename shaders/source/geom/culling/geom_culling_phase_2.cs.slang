#include "common/geom_culling_incl.slang"


[numthreads(GEOM_CULLING_CS_GROUP_SIZE, 1, 1)]
[shader("compute")]
void main(uint Gid : SV_GroupID, uint GI : SV_GroupIndex, uint DTid : SV_DispatchThreadID)
{
    ExecuteGeomCulling<GEOM_CULLING_PHASE::PHASE_2>(Gid, GI, DTid);
}