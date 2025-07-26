#pragma once

#include "platform/window.h"

#if defined(ENG_OS_WINDOWS)

#include "platform/win32/win32_headers.h"


class Win32Window final : public WindowBase
{
public:
    bool Init(const WindowCreateInfo& createInfo) override;
    void Destroy() override;

    uint32_t GetWidth() const override { return m_width; }; 
    uint32_t GetHeight() const override { return m_height; };
    
    void* GetNativeHandle() override { return m_hwnd; };
    const void* GetNativeHandle() const override { return m_hwnd; };

private:
    HWND m_hwnd = nullptr;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

#endif