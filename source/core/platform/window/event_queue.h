#pragma once

#include <variant>
#include <queue>

#include "core/utils/assert.h"


struct WndCloseEvent
{
};


struct WndActiveEvent
{
    WndActiveEvent(bool active)
        : isActive(active) {}

    bool isActive;
};


enum class WndResizeEventType : uint16_t
{
    RESTORED,
    MINIMIZED,
    MAXIMIZED,
};


struct WndResizeEvent
{
    WndResizeEvent(uint16_t w, uint16_t h, WndResizeEventType type)
        : width(w), height(h), type(type) {}

    bool IsMinimized() const noexcept { return type == WndResizeEventType::MINIMIZED; }
    bool IsMaximized() const noexcept { return type == WndResizeEventType::MAXIMIZED; }
    bool IsRestored() const noexcept { return type == WndResizeEventType::RESTORED; }

    uint16_t width;
    uint16_t height;
    WndResizeEventType type;
};


struct WndCursorEvent
{
    WndCursorEvent(int16_t x, int16_t y, bool isCtrlDown, bool isShiftDown, bool isLButtonDown, bool isMButtonDown, bool isRButtonDown)
        : x(x), y(y), isCtrlDown(isCtrlDown), isShiftDown(isShiftDown), isLButtonDown(isLButtonDown), isMButtonDown(isMButtonDown), isRButtonDown(isRButtonDown)
    {}

    int16_t x;
    int16_t y;

    struct {
        uint16_t isCtrlDown : 1;
        uint16_t isShiftDown : 1;
        uint16_t isLButtonDown : 1;
        uint16_t isMButtonDown : 1;
        uint16_t isRButtonDown : 1;
    };
};


#define ENG_DECLARE_WND_CURSOR_DERIVED_EVENT(EVENT_NAME)                                                                                  \
    struct EVENT_NAME : public WndCursorEvent                                                                                             \
    {                                                                                                                                     \
        EVENT_NAME(int16_t x, int16_t y, bool isCtrlDown, bool isShiftDown, bool isLButtonDown, bool isMButtonDown, bool isRButtonDown)   \
            : WndCursorEvent(x, y, isCtrlDown, isShiftDown, isLButtonDown, isMButtonDown, isRButtonDown) {}                               \
    }

ENG_DECLARE_WND_CURSOR_DERIVED_EVENT(WndMouseLButtonDownEvent);
ENG_DECLARE_WND_CURSOR_DERIVED_EVENT(WndMouseLButtonUpEvent);
ENG_DECLARE_WND_CURSOR_DERIVED_EVENT(WndMouseLButtonDblClkEvent);
ENG_DECLARE_WND_CURSOR_DERIVED_EVENT(WndMouseRButtonDownEvent);
ENG_DECLARE_WND_CURSOR_DERIVED_EVENT(WndMouseRButtonUpEvent);
ENG_DECLARE_WND_CURSOR_DERIVED_EVENT(WndMouseRButtonDblClkEvent);
ENG_DECLARE_WND_CURSOR_DERIVED_EVENT(WndMouseMButtonDownEvent);
ENG_DECLARE_WND_CURSOR_DERIVED_EVENT(WndMouseMButtonUpEvent);
ENG_DECLARE_WND_CURSOR_DERIVED_EVENT(WndMouseMButtonDblClkEvent);

#undef ENG_DECLARE_WND_CURSOR_DERIVED_EVENT

struct WndMouseWheelEvent : public WndCursorEvent
{
    WndMouseWheelEvent(int16_t delta, int16_t x, int16_t y, bool isCtrlDown, bool isShiftDown, bool isLButtonDown, bool isMButtonDown, bool isRButtonDown)
        : WndCursorEvent(x, y, isCtrlDown, isShiftDown, isLButtonDown, isMButtonDown, isRButtonDown), delta(delta) {}

    int16_t delta;
};


enum class WndKey : uint8_t
{
    KEY_SPACE,
    KEY_APOSTROPHE,
    KEY_COMMA,
    KEY_MINUS,
    KEY_DOT,
    KEY_SLASH,
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_SEMICOLON,
    KEY_EQUAL,
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    KEY_LEFT_BRACKET,
    KEY_BACKSLASH,
    KEY_RIGHT_BRACKET,
    KEY_GRAVE_ACCENT,
    KEY_ESCAPE,
    KEY_ENTER,
    KEY_TAB,
    KEY_BACKSPACE,
    KEY_INSERT,
    KEY_DELETE,
    KEY_RIGHT,
    KEY_LEFT,
    KEY_DOWN,
    KEY_UP,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_HOME,
    KEY_END,
    KEY_CAPS_LOCK,
    KEY_SCROLL_LOCK,
    KEY_NUM_LOCK,
    KEY_PRINT_SCREEN,
    KEY_PAUSE,
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_F13,
    KEY_F14,
    KEY_F15,
    KEY_F16,
    KEY_F17,
    KEY_F18,
    KEY_F19,
    KEY_F20,
    KEY_F21,
    KEY_F22,
    KEY_F23,
    KEY_F24,
    KEY_KP_0,
    KEY_KP_1,
    KEY_KP_2,
    KEY_KP_3,
    KEY_KP_4,
    KEY_KP_5,
    KEY_KP_6,
    KEY_KP_7,
    KEY_KP_8,
    KEY_KP_9,
    KEY_KP_DECIMAL,
    KEY_KP_DIVIDE,
    KEY_KP_MULTIPLY,
    KEY_KP_SUBTRACT,
    KEY_KP_ADD,
    KEY_LEFT_SHIFT,
    KEY_LEFT_CONTROL,
    KEY_LEFT_ALT,
    KEY_RIGHT_SHIFT,
    KEY_RIGHT_CONTROL,
    KEY_RIGHT_ALT,
    KEY_MEDIA_PREV_TRACK,
    KEY_MEDIA_NEXT_TRACK,
    KEY_MEDIA_PLAY_PAUSE,

    KEY_COUNT
};


enum class WndKeyState : uint8_t
{
    RELEASED,
    PRESSED,
    HOLD
};


struct WndKeyEvent
{
    WndKeyEvent(WndKey key, WndKeyState state)
        : key(key), state(state) {}

    bool IsPressed() const noexcept { return state == WndKeyState::PRESSED; }
    bool IsReleased() const noexcept { return state == WndKeyState::RELEASED; }
    bool IsHold() const noexcept { return state == WndKeyState::HOLD; }

    WndKey key;
    WndKeyState state;
};


using WndEventVariant = std::variant<
    WndCloseEvent, 
    WndActiveEvent, 
    WndResizeEvent, 
    WndCursorEvent, 
    WndMouseLButtonDownEvent,
    WndMouseLButtonUpEvent,
    WndMouseRButtonDownEvent,
    WndMouseRButtonUpEvent,
    WndMouseMButtonDownEvent,
    WndMouseMButtonUpEvent,
    WndMouseLButtonDblClkEvent,
    WndMouseRButtonDblClkEvent,
    WndMouseMButtonDblClkEvent,
    WndMouseWheelEvent,
    WndKeyEvent>;


class WndEvent
{
public:
    WndEvent() = default;

    template<typename EventType, typename... Args>
    WndEvent(Args&&... args)
        : m_event(std::in_place_type_t<EventType>(), std::forward<Args>(args)...)
    {}

    template<typename EventType>
    WndEvent(EventType&& event)
        : m_event(std::forward<EventType>(event))
    {}

    template<typename EventType, typename... Args>
    void Emplace(Args&&... args)
    { 
        m_event.emplace<EventType>(std::forward<Args>(args)...);
    }

    template<typename EventType>
    bool Is() const { return std::holds_alternative<EventType>(m_event); }
    
    template<typename EventType>
    const EventType& Get() const
    {
        ENG_ASSERT_PREFIX(Is<EventType>(), "CORE");
        ENG_ASSERT_PREFIX(IsValid(), "CORE");

        return std::get<EventType>(m_event);
    }

    bool IsValid() const { return m_event.index() != std::variant_npos; }

private:
    WndEventVariant m_event;
};


class WndEventQueue
{
public:
    WndEventQueue() = default;

    template<typename EventType, typename... Args>
    void Push(Args&&... args)
    {
        m_queue.emplace(EventType{std::forward<Args>(args)...});
    }

    bool Pop(WndEvent& outEvent)
    {
        if (IsEmpty()) {
            return false;
        }

        outEvent = m_queue.front();
        m_queue.pop();

        return true;
    }

    void Clear()
    {
        WndEvent event;
        while(Pop(event));
    }

    bool IsEmpty() const { return m_queue.empty(); }
    size_t GetSize() const { return m_queue.size(); }

private:
    std::queue<WndEvent> m_queue;
};


const char* wndWndKeyToStr(WndKey key) noexcept;