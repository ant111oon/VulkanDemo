#pragma once

#include "vk_device.h"
#include "vk_phys_device.h"


namespace vkn::utils
{
    void SetObjectName(Device& device, uint64_t objectHandle, VkObjectType objectType, const char* pObjectName);

    uint32_t FindMemoryType(const PhysicalDevice& physDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
}
