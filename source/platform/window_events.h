#pragma once

#include <variant>

#include "core/utils/assert.h"


struct WndCloseEvent
{
};


using WndEventVariant = std::variant<WndCloseEvent>;


class WndEvent
{
public:
    WndEvent() = default;

    template<typename EventType>
    WndEvent() { Emplace<EventType>(); }

    template<typename EventType>
    WndEvent(EventType&& event) { Emplace<EventType>(std::forward<EventType>(event)); }

    template<typename EventType, typename... Args>
    WndEvent(Args&&... args) { Emplace<EventType>(std::forward<Args>(args)...); }

    template<typename EventType>
    void Emplace() { m_event.emplace<EventType>(); }

    template<typename EventType>
    void Emplace(EventType&& event) { m_event.emplace<EventType>(std::forward<EventType>(event)); }

    template<typename EventType, typename... Args>
    void Emplace(Args&&... args) { m_event.emplace<EventType>(std::forward<Args>(args)...); }

    template<typename EventType>
    bool Is() const { return std::holds_alternative<EventType>(m_event); }
    
    template<typename EventType>
    const EventType& Get() const
    {
        ENG_ASSERT_PREFIX(Is<EventType>(), "CORE");
        return std::get<EventType>(m_event);
    }

    bool IsValid() const { return m_event.index() != std::variant_npos; }

private:
    WndEventVariant m_event;
};