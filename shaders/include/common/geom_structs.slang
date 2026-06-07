#ifndef GEOM_STRUCTS_H
#define GEOM_STRUCTS_H

enum GEOM_QUEUE : uint
{
    OPAQUE,
    AKILL,
    COUNT
};

#define GEOM_QUEUE_OPAQUE (uint)GEOM_QUEUE::OPAQUE
#define GEOM_QUEUE_AKILL  (uint)GEOM_QUEUE::AKILL
#define GEOM_QUEUE_COUNT  (uint)GEOM_QUEUE::COUNT


struct GEOM_BATCH
{
    uint MESH_ID;
    uint LOD_ID;
    uint FIRST_INST;
    uint INST_COUNT;
};


static const uint GEOM_CULLING_CS_GROUP_SIZE = 1024;
static const uint GEOM_BATCH_CS_GROUP_SIZE = 1024;
static const uint GEOM_DRAW_CMD_GEN_CS_GROUP_SIZE = 512;


#endif