#pragma once

#include "event_queue.h"

#include <string_view>
#include <cstdint>
#include <bitset>
#include <array>


struct WindowInitInfo
{
    std::string_view title;
    uint32_t width;
    uint32_t height;
};


class BaseWindow
{
public:
    BaseWindow() = default;
    virtual ~BaseWindow()
    { 
        Destroy();
    };

    virtual bool Init(const WindowInitInfo& initInfo) = 0;
    
    virtual void Destroy()
    {
        m_eventQueue.Clear();
        m_width = 0;
        m_height = 0;
        m_state = 0;
    }

    virtual void ProcessEvents() = 0;
    
    virtual void* GetNativeHandle() = 0;
    virtual const void* GetNativeHandle() const = 0;

    virtual void SetVisible(bool visible) = 0;

    bool PopEvent(WndEvent& event) { return m_eventQueue.Pop(event); }

    bool IsInitialized() const noexcept { return m_state.test(WND_STATE_INITIALIZED); }
    bool IsVisible() const noexcept { return m_state.test(WND_STATE_IS_VISIBLE); }
    bool IsClosed() const noexcept { return m_state.test(WND_STATE_IS_CLOSED); }
    bool IsActive() const noexcept { return m_state.test(WND_STATE_IS_ACTIVE); }
    bool IsMaximized() const noexcept { return m_state.test(WND_STATE_IS_MAXIMIZED); }
    bool IsMinimized() const noexcept { return m_state.test(WND_STATE_IS_MINIMIZED); }

    // Returns client area width
    uint32_t GetWidth() const noexcept { return m_width; }
    // Returns client area height
    uint32_t GetHeight() const noexcept { return m_height; }

protected:
    template <typename EventType, typename... Args>
    void PushEvent(Args&&... args)
    {
        m_eventQueue.Push<EventType>(std::forward<Args>(args)...);
    }

    void SetInitializedState(bool initialized) noexcept { m_state.set(WND_STATE_INITIALIZED, initialized); }
    void SetClosedState(bool closed) noexcept { m_state.set(WND_STATE_IS_CLOSED, closed); }
    void SetVisibleState(bool visible) noexcept { m_state.set(WND_STATE_IS_VISIBLE, visible); }
    void SetActiveState(bool active) noexcept { m_state.set(WND_STATE_IS_ACTIVE, active); }
    
    void ResetSizeState() noexcept
    { 
        m_state.set(WND_STATE_IS_MAXIMIZED, false);
        m_state.set(WND_STATE_IS_MINIMIZED, false);
    }

    void SetMinimizedState(bool minimized) noexcept
    {
        ResetSizeState();
        m_state.set(WND_STATE_IS_MINIMIZED, minimized);
    }

    void SetMaximizedState(bool maximized) noexcept
    { 
        ResetSizeState();
        m_state.set(WND_STATE_IS_MAXIMIZED, maximized);
    }

    void SetWidth(uint32_t width) noexcept { m_width = width; }
    void SetHeight(uint32_t height) noexcept { m_height = height; }

private:
    enum WndStateFlags : uint32_t
    {
        WND_STATE_INITIALIZED,
        WND_STATE_IS_CLOSED,
        WND_STATE_IS_VISIBLE,
        WND_STATE_IS_ACTIVE,
        WND_STATE_IS_MINIMIZED,
        WND_STATE_IS_MAXIMIZED,
    };

private:
    WndEventQueue m_eventQueue;

    uint32_t m_width = 0;
    uint32_t m_height = 0;

    std::bitset<32> m_state = 0;
};


#include "core/platform/platform.h"

#if defined(ENG_OS_WINDOWS)
    #include "core/platform/native/win32/window/win32_window.h" 
#endif