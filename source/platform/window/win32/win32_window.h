#pragma once

#include "platform/platform.h"

#if defined(ENG_OS_WINDOWS)

#include "platform/win32_headers.h"

#include <cstdint>


class Win32Window
{
public:
    uint32_t GetWidth() const noexcept { return m_width; }
    uint32_t GetHeight() const noexcept { return m_height; }

    HWND GetHWND() const noexcept { return m_hwnd; }
    HINSTANCE GetHINST() const noexcept { return m_hinst; }

private:
    HWND m_hwnd = nullptr;
    HINSTANCE m_hinst = nullptr;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

#endif