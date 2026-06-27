#ifndef SM_INCL_H
#define SM_INCL_H

#include "registers/sm_registers.slang"


uint SMGetQueueIndex(in uint cascadeIndex, in GEOM_QUEUE queue)
{
    return clamp(cascadeIndex, 0, CSM_CASCADE_COUNT - 1) * GEOM_QUEUE_COUNT + queue;
}

#endif