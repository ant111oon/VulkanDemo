#pragma once

#include "log.h" 

#include <cstdint>


#if defined(ENG_BUILD_DEBUG)
    template <typename... Args>
    inline void AssertImpl(const char* file, uint32_t line, const char* system, const char* fmt, Args&&... args) noexcept
    {
        Log(stderr, LogLevel::ERROR, file, line, system, fmt, std::forward<Args>(args)...);
        ENG_DEBUG_BREAK();
    }

    #define ENG_ASSERT_MSG(COND, SYSTEM, FMT, ...)                      \
        if (!(COND)) {                                                  \
            AssertImpl(__FILE__, __LINE__, SYSTEM, FMT, __VA_ARGS__);   \
        }

    #define ENG_ASSERT_SYSTEM(COND, SYSTEM) ENG_ASSERT_MSG(COND, SYSTEM, #COND)
    #define ENG_ASSERT(COND)                ENG_ASSERT_SYSTEM(COND, "GLOBAL")

    #define ENG_ASSERT_FAIL_SYSTEM(SYSTEM, FMT, ...) ENG_ASSERT_MSG(false, SYSTEM, FMT, __VA_ARGS__)
    #define ENG_ASSERT_FAIL(FMT, ...)                ENG_ASSERT_FAIL_SYSTEM("GLOBAL", FMT, __VA_ARGS__)
#else
    #define ENG_ASSERT_MSG(COND, SYSTEM, FMT, ...)
    #define ENG_ASSERT(COND)
    #define ENG_ASSERT_FAIL(FMT, ...)
    #define ENG_ASSERT_SYSTEM(COND, SYSTEM)
    #define ENG_ASSERT_FAIL_SYSTEM(SYSTEM, FMT, ...)
#endif