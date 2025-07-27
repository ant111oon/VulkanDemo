#pragma once

#include "core/platform/platform.h"

#include "log.h" 

#include <cstdint>
#include <string_view>


#if defined(ENG_BUILD_DEBUG)
    template <typename... Args>
    inline void AssertImpl(std::string_view file, uint32_t line, std::string_view prefix, std::string_view fmt, Args&&... args) noexcept
    {
        Log(stderr, LogLevel::ERROR, file, line, prefix, fmt, std::forward<Args>(args)...);
        ENG_DEBUG_BREAK();
    }

    #define ENG_ASSERT_MSG(COND, PREFIX, FMT, ...)                      \
        if (!(COND)) {                                                  \
            AssertImpl(__FILE__, __LINE__, PREFIX, FMT, __VA_ARGS__);   \
        }

    #define ENG_ASSERT_PREFIX(COND, PREFIX) ENG_ASSERT_MSG(COND, PREFIX, #COND)
    #define ENG_ASSERT(COND)                ENG_ASSERT_PREFIX(COND, "GLOBAL")

    #define ENG_ASSERT_FAIL_PREFIX(PREFIX, FMT, ...) ENG_ASSERT_MSG(false, PREFIX, FMT, __VA_ARGS__)
    #define ENG_ASSERT_FAIL(FMT, ...)                ENG_ASSERT_FAIL_PREFIX("GLOBAL", FMT, __VA_ARGS__)
#else
    #define ENG_ASSERT_PREFIX(COND, PREFIX)
    #define ENG_ASSERT(COND)
    #define ENG_ASSERT_FAIL_PREFIX(PREFIX, FMT, ...)
    #define ENG_ASSERT_FAIL(FMT, ...)
#endif