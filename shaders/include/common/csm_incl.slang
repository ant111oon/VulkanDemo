#ifndef CSM_INCL_H
#define CSM_INCL_H

#include "registers/csm_registers.slang"


uint CSMGetBufferIndex(in uint cascade, in GPU_GeomQueue queue)
{
    return clamp(cascade, 0, COMMON_CSM_CASCADE_COUNT - 1) * GEOM_QUEUE_COUNT + queue;
}


uint CSMGeomCullingGetQueueMask(in GPU_GeomInst inst)
{
    uint mask = 0;

    const GPU_GeomMaterial material = COMMON_MATERIALS[inst.materialID];

    if (material.IsOpaque()) {
        mask |= (1u << GEOM_QUEUE_OPAQUE);
    } else if (material.IsAlphaKill()) {
        mask |= (1u << GEOM_QUEUE_AKILL);
    }

    return mask;
}

#endif