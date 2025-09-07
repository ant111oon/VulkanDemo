#pragma once

#include "vk_core.h"


namespace vkn::utils
{
    void SetObjectName(VkDevice vkDevice, uint64_t objectHandle, VkObjectType objectType, const char* pObjectName);
}
