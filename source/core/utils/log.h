#pragma once

#include <string_view>
#include <array>

#include <cstdio>
#include <cstdint>


#if defined(ENG_BUILD_DEBUG) || defined(ENG_BUILD_PROFILE)
    #define ENG_LOGGING_ENABLED
#endif


namespace eng
{
    enum class LogLevel { TRACE, INFO, WARN, ERROR };
    
    
    void LogInternal(FILE* pStream, LogLevel level, const char* file, uint32_t line, const char* system, const char* fmt, ...) noexcept;
    
    
    template <typename... Args>
    inline void Log(FILE* pStream, LogLevel level, const char* file, uint32_t line, const char* system, const char* fmt, Args&&... args) noexcept
    {
        LogInternal(pStream, level, file, line, system, fmt, std::forward<Args>(args)...);
    }
}


#ifdef ENG_LOGGING_ENABLED
    #define ENG_LOG_TRACE(SYSTEM, FMT, ...) eng::Log(stdout, eng::LogLevel::TRACE, __FILE__, __LINE__, SYSTEM, FMT, __VA_ARGS__)
    #define ENG_LOG_INFO(SYSTEM, FMT, ...)  eng::Log(stdout, eng::LogLevel::INFO, __FILE__, __LINE__, SYSTEM, FMT, __VA_ARGS__)
    #define ENG_LOG_WARN(SYSTEM, FMT, ...)  eng::Log(stdout, eng::LogLevel::WARN, __FILE__, __LINE__, SYSTEM, FMT, __VA_ARGS__)
    #define ENG_LOG_ERROR(SYSTEM, FMT, ...) eng::Log(stderr, eng::LogLevel::ERROR, __FILE__, __LINE__, SYSTEM, FMT, __VA_ARGS__)
#else
    #define ENG_LOG_TRACE(SYSTEM, FMT, ...)
    #define ENG_LOG_INFO(SYSTEM, FMT, ...)
    #define ENG_LOG_WARN(SYSTEM, FMT, ...)
    #define ENG_LOG_ERROR(SYSTEM, FMT, ...)
#endif