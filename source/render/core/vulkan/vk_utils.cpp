#include "pch.h"

#include "vk_utils.h"
#include "vk_instance.h"


namespace vkn::utils
{ 
    void SetObjectName(Device& device, uint64_t objectHandle, VkObjectType objectType, const char* pObjectName)
    {
        static PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectName = nullptr;

        if (!vkSetDebugUtilsObjectName) {
            vkSetDebugUtilsObjectName = (PFN_vkSetDebugUtilsObjectNameEXT)GetInstance().GetProcAddr("vkSetDebugUtilsObjectNameEXT");
        }

        VK_ASSERT(pObjectName != nullptr);

        VkDebugUtilsObjectNameInfoEXT dbgUtilsObjNameInfo = {};
        dbgUtilsObjNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        dbgUtilsObjNameInfo.objectHandle = objectHandle;
        dbgUtilsObjNameInfo.objectType = objectType;
        dbgUtilsObjNameInfo.pObjectName = pObjectName;

        VK_CHECK(vkSetDebugUtilsObjectName(device.Get(), &dbgUtilsObjNameInfo));
    }


    uint32_t FindMemoryType(const PhysicalDevice& physDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        const VkPhysicalDeviceMemoryProperties memProps = physDevice.GetMemoryProperties();

        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            const VkMemoryPropertyFlags propertyFlags = memProps.memoryTypes[i].propertyFlags;
            
            if ((typeFilter & (1 << i)) && (propertyFlags & properties) == properties) {
                return i;
            }
        }

        return UINT32_MAX;
    }
}