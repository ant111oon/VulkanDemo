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


    VkImageViewType ImageTypeToViewType(VkImageType type)
    {
        switch(type) {
            case VK_IMAGE_TYPE_1D:
                return VK_IMAGE_VIEW_TYPE_1D;
            case VK_IMAGE_TYPE_2D:
                return VK_IMAGE_VIEW_TYPE_2D;
            case VK_IMAGE_TYPE_3D:
                return VK_IMAGE_VIEW_TYPE_3D;
            default:
                VK_ASSERT_FAIL("Invalid Vulkan image type");
                return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
        }
    }
}