#pragma once

#ifdef ENG_GFX_API_VULKAN
    #define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
 