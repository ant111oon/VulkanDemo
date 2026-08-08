#include "common/geom_culling_incl.slang"


[numthreads(GEOM_CULLING_CS_GROUP_SIZE, 1, 1)]
[shader("compute")]
void main(in uint Gid : SV_GroupID, in uint GI : SV_GroupIndex, in uint DTid : SV_DispatchThreadID)
{
    ExecuteGeomCulling(Gid, GI, DTid);
}