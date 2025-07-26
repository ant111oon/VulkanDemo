#pragma once

#include <cstdint>
#include <string_view>
#include <bitset>


struct WindowInitInfo
{
    std::string_view title;
    uint32_t width;
    uint32_t height;
};


class WindowBase
{
public:
    WindowBase() = default;
    virtual ~WindowBase()
    { 
        Destroy();
    };

    virtual bool Init(const WindowInitInfo& initInfo) = 0;
    virtual void Destroy()
    {
        m_width = 0;
        m_height = 0;
        m_state = 0;
    }

    virtual void PollEvents() const = 0;
    
    virtual void* GetNativeHandle() = 0;
    virtual const void* GetNativeHandle() const = 0;

    virtual void SetVisible(bool visible) = 0;

    bool IsInitialized() const noexcept { return m_state.test(WND_STATE_INITIALIZED); }; 
    bool IsVisible() const noexcept { return m_state.test(WND_STATE_IS_VISIBLE); }; 
    bool IsClosed() const noexcept { return m_state.test(WND_STATE_IS_CLOSED); }; 

    uint32_t GetWidth() const noexcept { return m_width; }; 
    uint32_t GetHeight() const noexcept { return m_height; };

protected:
    void SetInitializedState(bool initialized) noexcept { m_state.set(WND_STATE_INITIALIZED, initialized); }
    void SetVisibleState(bool visible) noexcept { m_state.set(WND_STATE_IS_VISIBLE, visible); }
    void SetClosedState(bool closed = true) noexcept { m_state.set(WND_STATE_IS_CLOSED, closed); }

    void SetWidth(uint32_t width) noexcept { m_width = width; }
    void SetHeight(uint32_t height) noexcept { m_height = height; }

private:
    enum WndStateFlags : uint32_t
    {
        WND_STATE_INITIALIZED,
        WND_STATE_IS_VISIBLE,
        WND_STATE_IS_CLOSED,
    };

private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    std::bitset<32> m_state = 0;
};


#include "platform.h"

#if defined(ENG_OS_WINDOWS)
    #include "win32/window/win32_window.h" 
#endif