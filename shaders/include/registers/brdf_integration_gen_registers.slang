#ifndef BRDF_INTEGRATION_GEN_H
#define BRDF_INTEGRATION_GEN_H

#include "common/system/sys_constants.slang"


[vk::binding(0, DESC_SET_PER_DRAW)] RWTexture2D BRDF_INTEGRATION_GEN_OUTPUT_UAV;

#endif