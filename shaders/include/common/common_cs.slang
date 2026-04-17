#ifndef COMMON_CS_H
#define COMMON_CS_H

int2 DispatchThreadIDToPixCoord(in uint2 dtid, in uint2 size)
{
    return int2(int(dtid.x), int(size.y) - 1 - dtid.y);
}

#endif