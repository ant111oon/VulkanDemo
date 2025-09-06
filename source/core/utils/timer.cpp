#include "pch.h"

#include "timer.h"


Timer::Timer()
{
    Start();
}


Timer& Timer::Reset()
{ 
    m_start = m_end = std::chrono::high_resolution_clock::now();
    return *this;
}


Timer& Timer::Start()
{
    m_start = std::chrono::high_resolution_clock::now();
    return *this;
}


Timer& Timer::End()
{
    m_end = std::chrono::high_resolution_clock::now();
    return *this;
}