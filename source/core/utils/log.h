#pragma once

#include <string_view>
#include <array>

#include <cstdio>
#include <cstdint>


enum class LogLevel { TRACE, INFO, WARN, ERROR };


void LogInternal(FILE* pStream, LogLevel level, const char* file, uint32_t line, const char* system, const char* fmt, ...) noexcept;


template <typename... Args>
inline void Log(FILE* pStream, LogLevel level, const char* file, uint32_t line, const char* system, const char* fmt, Args&&... args) noexcept
{
    LogInternal(pStream, level, file, line, system, fmt, std::forward<Args>(args)...);
}


#define ENG_LOG_TRACE(SYSTEM, FMT, ...) Log(stdout, LogLevel::TRACE, __FILE__, __LINE__, SYSTEM, FMT, __VA_ARGS__)
#define ENG_LOG_INFO(SYSTEM, FMT, ...)  Log(stdout, LogLevel::INFO, __FILE__, __LINE__, SYSTEM, FMT, __VA_ARGS__)
#define ENG_LOG_WARN(SYSTEM, FMT, ...)  Log(stdout, LogLevel::WARN, __FILE__, __LINE__, SYSTEM, FMT, __VA_ARGS__)
#define ENG_LOG_ERROR(SYSTEM, FMT, ...) Log(stderr, LogLevel::ERROR, __FILE__, __LINE__, SYSTEM, FMT, __VA_ARGS__)