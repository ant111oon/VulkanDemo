#pragma once

#include "utils/assert.h"


#define CORE_LOG_TRACE(FMT, ...)        ENG_LOG_TRACE("CORE", FMT, __VA_ARGS__)
#define CORE_LOG_INFO(FMT, ...)         ENG_LOG_INFO("CORE",  FMT, __VA_ARGS__)
#define CORE_LOG_WARN(FMT, ...)         ENG_LOG_WARN("CORE",  FMT, __VA_ARGS__)
#define CORE_LOG_ERROR(FMT, ...)        ENG_LOG_ERROR("CORE", FMT, __VA_ARGS__)
#define CORE_ASSERT_MSG(COND, FMT, ...) ENG_ASSERT_MSG(COND, "CORE", FMT, __VA_ARGS__)
#define CORE_ASSERT(COND)               VK_ASSERT_MSG(COND, #COND)
#define CORE_ASSERT_FAIL(FMT, ...)      VK_ASSERT_MSG(false, FMT, __VA_ARGS__)