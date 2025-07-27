#pragma once

#include "core/platform/platform.h"

#include <cstdint>
#include <string_view>


#if defined(ENG_BUILD_DEBUG)
    template <typename... Args>
    inline void AssertImpl(std::string_view file, uint32_t line, std::string_view prefix, std::string_view fmt, Args&&... args) noexcept
    {
        char msgBuffer[4096] = {0};
        
        const size_t msgBufferSize = sizeof(msgBuffer);
        size_t actualMsgBufferSize = 0;

        if (!prefix.empty()) {
            actualMsgBufferSize += sprintf_s(msgBuffer + actualMsgBufferSize, msgBufferSize - actualMsgBufferSize, 
                "[" "\x1b[33m" "%*s" "\x1b[0m" "]: ", prefix.size(), prefix.data());
        }

        char format[2048] = {0};
        sprintf_s(format, "\x1b[31m" "%*s" "\x1b[0m", fmt.size(), fmt.data());

        actualMsgBufferSize += sprintf_s(msgBuffer + actualMsgBufferSize, msgBufferSize - actualMsgBufferSize, 
            format, std::forward<Args>(args)...);
        actualMsgBufferSize += sprintf_s(msgBuffer + actualMsgBufferSize, msgBufferSize - actualMsgBufferSize, 
            " (%*s:%u)\n", file.size(), file.data(), line);
        
        fprintf_s(stderr, msgBuffer);

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