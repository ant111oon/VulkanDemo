#pragma once

#include "event_queue.h"

#include <string_view>
#include <cstdint>
#include <bitset>
#include <array>

#include <memory>


struct WindowInitInfo
{
    const char* pTitle;
    uint32_t width;
    uint32_t height;
    bool isVisible;
};


class Window
{
public:
    Window() = default;
    virtual ~Window() = default;

    virtual bool Create(const WindowInitInfo& initInfo) = 0;
    
    virtual void Destroy()
    {
        m_eventQueue.Clear();
        m_title.fill('\0');
        m_width = 0;
        m_height = 0;
        m_state.reset();
    }

    virtual void PullEvents() = 0;
    
    virtual void* GetNativeHandle() = 0;
    virtual const void* GetNativeHandle() const = 0;

    virtual void SetVisible(bool visible) = 0;
    virtual void SetCursorHidden(bool hidden) = 0;

    virtual void SetCursorRelativeMode(bool relative) = 0;

    template <typename... Args>
    void SetTitle(const char* fmt, Args&&... args)
    {
        SetTitleData(fmt, std::forward<Args>(args)...);
        UpdateTitleInternal();
    }

    bool PopEvent(WndEvent& event) { return m_eventQueue.Pop(event); }

    bool IsInitialized() const noexcept { return m_state.test(WND_STATE_INITIALIZED); }
    bool IsVisible() const noexcept { return m_state.test(WND_STATE_IS_VISIBLE); }
    bool IsCursorHidden() const noexcept { return m_state.test(WND_STATE_IS_CURSOR_HIDDEN); }
    bool IsCursorRelativeMode() const noexcept { return m_state.test(WND_STATE_IS_CURSOR_RELATIVE_MODE); }
    bool IsClosed() const noexcept { return m_state.test(WND_STATE_IS_CLOSED); }
    bool IsActive() const noexcept { return m_state.test(WND_STATE_IS_ACTIVE); }
    bool IsMaximized() const noexcept { return m_state.test(WND_STATE_IS_MAXIMIZED); }
    bool IsMinimized() const noexcept { return m_state.test(WND_STATE_IS_MINIMIZED); }

    // Returns client area width
    uint32_t GetWidth() const noexcept { return m_width; }
    // Returns client area height
    uint32_t GetHeight() const noexcept { return m_height; }

    int16_t GetPrevCursorX() const noexcept { return m_prevCursorX; }
    int16_t GetPrevCursorY() const noexcept { return m_prevCursorY; }
    int16_t GetCursorX() const noexcept { return m_cursorX; }
    int16_t GetCursorY() const noexcept { return m_cursorY; }

    int16_t GetCursorDX() const noexcept { return GetCursorX() - GetPrevCursorX(); }
    int16_t GetCursorDY() const noexcept { return GetCursorY() - GetPrevCursorY(); }

    std::string_view GetTitle() const noexcept { return std::string_view(m_title.data(), strlen(m_title.data())); }

protected:
    virtual void UpdateTitleInternal() = 0;

protected:
    template <typename EventType, typename... Args>
    void PushEvent(Args&&... args)
    {
        m_eventQueue.Push<EventType>(std::forward<Args>(args)...);
    }

    void SetInitializedState(bool initialized) noexcept { m_state.set(WND_STATE_INITIALIZED, initialized); }
    void SetClosedState(bool closed) noexcept { m_state.set(WND_STATE_IS_CLOSED, closed); }
    void SetVisibleState(bool visible) noexcept { m_state.set(WND_STATE_IS_VISIBLE, visible); }
    void SetCursorHiddenState(bool hidden) noexcept { m_state.set(WND_STATE_IS_CURSOR_HIDDEN, hidden); }
    void SetCursorRelativeModeState(bool relative) noexcept { m_state.set(WND_STATE_IS_CURSOR_RELATIVE_MODE, relative); }
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

    void SetPrevCursorX(int16_t x) noexcept { m_prevCursorX = x; }
    void SetPrevCursorY(int16_t y) noexcept { m_prevCursorY = y; }
    void SetPrevCursorXY(int16_t x, int16_t y) noexcept
    {
        SetPrevCursorX(x);
        SetPrevCursorY(y);
    }

    void SetCursorX(int16_t x) noexcept { m_cursorX = x; }
    void SetCursorY(int16_t y) noexcept { m_cursorY = y; }
    void SetCursorXY(int16_t x, int16_t y) noexcept
    {
        SetCursorX(x);
        SetCursorY(y);
    }

    void SetTitleData(const char* title)
    {
        strncpy_s(m_title.data(), m_title.size(), title ? title : "", _TRUNCATE);
    }

    template <typename... Args>
    void SetTitleData(const char* fmt, Args&&... args)
    {
        sprintf_s(m_title.data(), m_title.size(), fmt, std::forward<Args>(args)...);
    }

private:
    enum WndStateFlags : uint32_t
    {
        WND_STATE_INITIALIZED,
        WND_STATE_IS_CLOSED,
        WND_STATE_IS_VISIBLE,
        WND_STATE_IS_CURSOR_HIDDEN,
        WND_STATE_IS_CURSOR_RELATIVE_MODE,
        WND_STATE_IS_ACTIVE,
        WND_STATE_IS_MINIMIZED,
        WND_STATE_IS_MAXIMIZED,
    };

    static inline constexpr size_t MAX_WND_NAME_LENGTH = 256; 

private:
    WndEventQueue m_eventQueue;

    std::array<char, MAX_WND_NAME_LENGTH> m_title = {};

    uint32_t m_width = 0;
    uint32_t m_height = 0;

    int16_t m_cursorX = 0;
    int16_t m_cursorY = 0;
    int16_t m_prevCursorX = 0;
    int16_t m_prevCursorY = 0;

    std::bitset<32> m_state = 0;
};


std::unique_ptr<Window> AllocateWindow();
