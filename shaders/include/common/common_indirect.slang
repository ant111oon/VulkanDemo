#ifndef COMMON_INDIRECT_H
#define COMMON_INDIRECT_H


struct GPU_CmdDrawIndexedIndirect
{
    // NOTE: Don't change order of this variables!!!
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};


struct GPU_CmdDispatchIndirect
{
    // NOTE: Don't change order of this variables!!!
    uint groupSizeX;
    uint groupSizeY;
    uint groupSizeZ;
};


#endif