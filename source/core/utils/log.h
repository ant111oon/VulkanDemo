#pragma once

#include <string_view>
#include <array>

#include <cstdio>
#include <cstdint>


enum class LogLevel { TRACE, INFO, WARN, ERROR };


template <typename... Args>
inline void Log(FILE* pStream, LogLevel level, std::string_view file, uint32_t line, std::string_view prefix, std::string_view fmt, Args&&... args) noexcept
{
    if (!pStream) {
        return;
    }

    static constexpr const char* RESET_COLOR = "\x1b[0m";
    static constexpr const char* WHITE_COLOR = "\x1b[37m";
    static constexpr const char* RED_COLOR = "\x1b[31m";
    static constexpr const char* GREEN_COLOR = "\x1b[32m";
    static constexpr const char* YELLOW_COLOR = "\x1b[33m";

    static auto LogLevelToColor = [=](LogLevel level) -> const char*
    {
        switch (level) {
            case LogLevel::TRACE: return RESET_COLOR;
            case LogLevel::INFO: return GREEN_COLOR;
            case LogLevel::WARN: return YELLOW_COLOR;
            case LogLevel::ERROR: return RED_COLOR;
            default: return RESET_COLOR;
        };
    };

    static auto LogLevelToMarker = [=](LogLevel level) -> const char*
    {
        switch (level) {
            case LogLevel::TRACE: return "TRACE";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARN: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            default: return "";
        };
    };

    static constexpr size_t MAX_LOG_MSG_LENGTH = 4096;
    static constexpr size_t MAX_FMT_LENGTH = 2048;

    const char* pLogColor = LogLevelToColor(level);
    const char* pMarker = LogLevelToMarker(level);

    std::array<char, MAX_LOG_MSG_LENGTH> msgBuffer = {};

    size_t actualMsgBufferSize = sprintf_s(msgBuffer.data(), msgBuffer.size(), "[%s%s%s] ", pLogColor, pMarker, RESET_COLOR);
    size_t remainMsgBufferSize = msgBuffer.size() - actualMsgBufferSize;

    if (!prefix.empty()) {
        actualMsgBufferSize += sprintf_s(msgBuffer.data() + actualMsgBufferSize, remainMsgBufferSize, "[%*s]: ", prefix.size(), prefix.data());
        remainMsgBufferSize = msgBuffer.size() - actualMsgBufferSize;
    }

    std::array<char, MAX_FMT_LENGTH> format = {};
    sprintf_s(format.data(), format.size(), "%s%*s%s", pLogColor, fmt.size(), fmt.data(), RESET_COLOR);

    actualMsgBufferSize += sprintf_s(msgBuffer.data() + actualMsgBufferSize, remainMsgBufferSize, format.data(), std::forward<Args>(args)...);
    remainMsgBufferSize = msgBuffer.size() - actualMsgBufferSize;

    actualMsgBufferSize += sprintf_s(msgBuffer.data() + actualMsgBufferSize, remainMsgBufferSize, " (%*s:%u)\n", file.size(), file.data(), line);
    remainMsgBufferSize = msgBuffer.size() - actualMsgBufferSize;
        
    fprintf_s(pStream, msgBuffer.data());
}


#define ENG_LOG_TRACE(PREFIX, FMT, ...) Log(stdout, LogLevel::TRACE, __FILE__, __LINE__, PREFIX, FMT, __VA_ARGS__)
#define ENG_LOG_INFO(PREFIX, FMT, ...)  Log(stdout, LogLevel::INFO, __FILE__, __LINE__, PREFIX, FMT, __VA_ARGS__)
#define ENG_LOG_WARN(PREFIX, FMT, ...)  Log(stdout, LogLevel::WARN, __FILE__, __LINE__, PREFIX, FMT, __VA_ARGS__)
#define ENG_LOG_ERROR(PREFIX, FMT, ...) Log(stderr, LogLevel::ERROR, __FILE__, __LINE__, PREFIX, FMT, __VA_ARGS__)