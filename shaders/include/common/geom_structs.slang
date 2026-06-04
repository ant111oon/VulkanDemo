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

#endif