#pragma once

#if !defined(ENG_GFX_API_VULKAN)
    #error Invalid Gpraphics API defines
#endif


#if defined(ENG_BUILD_DEBUG) || defined(ENG_BUILD_PROFILE)
    #define ENG_VK_DEBUG_UTILS_ENABLED
#endif


#if defined(ENG_BUILD_DEBUG) || defined(ENG_BUILD_PROFILE)
    #define ENG_VK_OBJ_DEBUG_NAME_ENABLED
#endif


#include "core/platform/platform.h"

#include "core/utils/assert.h"
#include "core/core.h"

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#ifdef ENG_OS_WINDOWS
    #include <vulkan/vulkan_win32.h>
#endif


#define VK_CHECK(VkCall)                                                                  \
    do {                                                                                  \
        const VkResult _vkCallResult = VkCall;                                            \
        (void)_vkCallResult;                                                              \
        VK_ASSERT_MSG(_vkCallResult == VK_SUCCESS, "%s", string_VkResult(_vkCallResult)); \
    } while(0)


#define VK_LOG_TRACE(FMT, ...)        ENG_LOG_TRACE("VULKAN", FMT, __VA_ARGS__)
#define VK_LOG_INFO(FMT, ...)         ENG_LOG_INFO("VULKAN",  FMT, __VA_ARGS__)
#define VK_LOG_WARN(FMT, ...)         ENG_LOG_WARN("VULKAN",  FMT, __VA_ARGS__)
#define VK_LOG_ERROR(FMT, ...)        ENG_LOG_ERROR("VULKAN", FMT, __VA_ARGS__)

#define VK_ASSERT_MSG(COND, FMT, ...) ENG_ASSERT_MSG(COND, "VULKAN", FMT, __VA_ARGS__)
#define VK_ASSERT(COND)               VK_ASSERT_MSG(COND, #COND)
#define VK_ASSERT_FAIL(FMT, ...)      VK_ASSERT_MSG(false, FMT, __VA_ARGS__)

