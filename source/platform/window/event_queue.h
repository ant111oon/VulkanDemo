#pragma once

#include <variant>
#include <queue>

#include "core/utils/assert.h"


struct WndCloseEvent
{
};


using WndEventVariant = std::variant<WndCloseEvent>;


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

    bool IsEmpty() const { return m_queue.empty(); }
    size_t GetSize() const { return m_queue.size(); }

private:
    std::queue<WndEvent> m_queue;
};