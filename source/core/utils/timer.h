#pragma once

#include "core/core.h"

#include <chrono>


class Timer
{
public:
    Timer();

    Timer& Reset();

    Timer& Start();
    Timer& End();

    template<typename RESULT_T, typename DURATION_T>
    RESULT_T GetDuration() const
    {
        CORE_ASSERT_MSG(m_end > m_start, "Need to call End() before GetDuration()");
        return std::chrono::duration<RESULT_T, DURATION_T>(m_end - m_start).count();
    }

    template<typename RESULT_T, typename DURATION_T>
    Timer& GetDuration(RESULT_T& outResult)
    {
        outResult = GetDuration<RESULT_T, DURATION_T>();
        return *this;
    }

private:
    using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

    TimePoint m_start;
    TimePoint m_end;
};