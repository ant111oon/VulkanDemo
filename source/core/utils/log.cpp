#include "pch.h"
#include "log.h"

#include "core/profiler/cpu_profiler.h"


static constexpr const char* LogLevelToStr(LogLevel level)
{
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
    };

    return "UNKNOWN";
}


#ifdef ENG_OS_WINDOWS
enum OutputColor : WORD
{
    OUTPUT_COLOR_DEFAULT = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
    OUTPUT_COLOR_RED = FOREGROUND_RED,
    OUTPUT_COLOR_GREEN = FOREGROUND_GREEN,
    OUTPUT_COLOR_BLUE = FOREGROUND_BLUE,
    OUTPUT_COLOR_YELLOW = FOREGROUND_RED | FOREGROUND_GREEN,
};


static constexpr WORD LogLevelToColor(LogLevel level)
{
    switch (level) {
        case LogLevel::TRACE: return OUTPUT_COLOR_DEFAULT;
        case LogLevel::INFO:  return OUTPUT_COLOR_GREEN;
        case LogLevel::WARN:  return OUTPUT_COLOR_YELLOW;
        case LogLevel::ERROR: return OUTPUT_COLOR_RED;
    };

    return OUTPUT_COLOR_DEFAULT;
}


void LogInternal(FILE* pStream, LogLevel level, const char* file, uint32_t line, const char* system, const char* fmt, ...) noexcept
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    const WORD logColor = LogLevelToColor(level);

    fputc('[', pStream);
    SetConsoleTextAttribute(hConsole, logColor);
        fputs(LogLevelToStr(level), pStream);
    SetConsoleTextAttribute(hConsole, OUTPUT_COLOR_DEFAULT);
    fputs("] ", pStream);

    if (system) {
        fprintf_s(pStream, "[%s]: ", system);
    }

    va_list args;
    va_start(args, fmt);
        SetConsoleTextAttribute(hConsole, logColor);
            vfprintf_s(pStream, fmt, args);
        SetConsoleTextAttribute(hConsole, OUTPUT_COLOR_DEFAULT);

    #ifdef ENG_PROFILING_ENABLED
        char profilerMsgBuffer[2048] = {'\0'};
        vsprintf_s(profilerMsgBuffer, fmt, args);
        
        switch (level) {
            case LogLevel::TRACE:
                ENG_PROFILE_LOG_C(profilerMsgBuffer, 255, 255, 255, 255);
                break;
            case LogLevel::INFO:
                ENG_PROFILE_LOG_C(profilerMsgBuffer, 0, 255, 0, 255);
                break;
            case LogLevel::WARN:
                ENG_PROFILE_LOG_C(profilerMsgBuffer, 255, 255, 0, 255);
                break;
            case LogLevel::ERROR:
                ENG_PROFILE_LOG_C(profilerMsgBuffer, 255, 0, 0, 255);
                break;
        }
    #endif
    va_end(args);

    fprintf_s(pStream, " (%s:%u)\n", file, line);
}
#else
    #error Invalid platform
#endif