#ifndef GAUSSIAN_BLUR_REGISTERS_H
#define GAUSSIAN_BLUR_REGISTERS_H

#include "common/system/sys_constants.slang"


static const uint GAUSSIAN_BLUR_CS_GROUP_SIZE_X = 16;
static const uint GAUSSIAN_BLUR_CS_GROUP_SIZE_Y = 16;

enum GPU_GaussianBlurPass
{
    HORIZONTAL,
    VERTICAL,
};

struct GPU_GaussianBlurPushConst
{
    GPU_GaussianBlurPass axis;
};

[[vk::push_constant]] GPU_GaussianBlurPushConst GAUSSIAN_BLUR_CONST;

#define GAUSSIAN_BLUR_PASS GAUSSIAN_BLUR_CONST.pass


[vk::binding(0, DESC_SET_PER_DRAW)] Texture2D   GAUSSIAN_BLUR_SRC;
[vk::binding(1, DESC_SET_PER_DRAW)] RWTexture2D GAUSSIAN_BLUR_DST;


#endif