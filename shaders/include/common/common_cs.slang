#ifndef COMMON_CS_H
#define COMMON_CS_H

int2 DispatchThreadIDToPixCoord(in uint2 dtid, in uint2 size)
{
    return int2(min(dtid.x, size.x), max(size.y - 1, dtid.y) - dtid.y);
}

#endif