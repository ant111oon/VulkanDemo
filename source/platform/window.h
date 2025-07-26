#pragma once

#include <cstdint>
#include <string_view>


struct WindowCreateInfo
{
    std::string_view title;
    uint32_t width;
    uint32_t height;
};


class WindowBase
{
public:
    WindowBase() = default;
    virtual ~WindowBase() = default;

    virtual bool Init(const WindowCreateInfo& createInfo) = 0;
    virtual void Destroy() = 0;

    virtual uint32_t GetWidth() const = 0; 
    virtual uint32_t GetHeight() const = 0;
    
    virtual void* GetNativeHandle() = 0;
    virtual const void* GetNativeHandle() const = 0;
};


#include "platform.h"

#if defined(ENG_OS_WINDOWS)
    #include "win32/window/win32_window.h" 
#endif