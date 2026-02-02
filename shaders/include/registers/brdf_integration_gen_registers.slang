#ifndef BRDF_INTEGRATION_GEN_H
#define BRDF_INTEGRATION_GEN_H

#include "common_registers.slang"


struct BRDF_INTEGRATION_PUSH_CONSTS
{
    uint4 PADDING; // Free for now
};

[[vk::push_constant]] BRDF_INTEGRATION_PUSH_CONSTS PUSH_CONSTS;


[vk::binding(0, 1)] RWTexture2D BRDF_INTEGRATION_GEN_OUTPUT_UAV;

#endif