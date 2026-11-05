// Inspired by code from https://denyskryvytskyi.github.io/event-system

#pragma once

#include <svanes/utility/hash.hpp>

#include <cstdint>
#include <string_view>

namespace svanes {

using EventPriority = std::uint64_t;

// Identifies an event type at runtime. It is the hash of the type's EventName,
// so it stays the same across runs and platforms.
using EventTypeId = std::uint64_t;

/**
 * Returns the ID of an event type. Always evaluated at compile time, so the
 * result can be used as a case label.
 *
 * @tparam T The event type. It must declare a static constexpr EventName.
 *
 * @return The hash of T::EventName.
 */
template <typename T> consteval EventTypeId EventTypeIdOf() {
    return HashText(T::EventName);
}

/**
 * Represents any event that can be handled by the application.
 */
class Event {
public:
    /**
     * Default destructor for the Event class.
     */
    virtual ~Event() = default;

    /**
     * Returns the readable name of the event's type. Event types get this from
     * EventOf rather than implementing it themselves.
     *
     * @return The EventName of the event's type.
     */
    virtual std::string_view GetEventType() const = 0;

    /**
     * Returns the ID of the event's type. Event types get this from EventOf
     * rather than implementing it themselves.
     *
     * @return The ID used to find the handlers and replay codec for this event.
     */
    virtual EventTypeId GetTypeId() const = 0;

    /**
     * The dispatch priority. Smaller values are processed first.
     */
    EventPriority priority = 0;

    /**
     * Indicates whether the event has been handled by any event handler.
     */
    bool Handled{false};
};

/**
 * The base class for concrete event types. It implements Event's type functions
 * from the derived class's EventName. Each event type must have a unique
 * EventName, because code downcasts events using a static_cast once their ID
 * has matched.
 *
 * @tparam T The event type deriving from this class. It must declare a static
 * constexpr std::string_view EventName.
 */
template <typename T> class EventOf : public Event {
public:
    /**
     * Returns the readable name of the event's type.
     *
     * @return T::EventName.
     */
    std::string_view GetEventType() const final { return T::EventName; }

    /**
     * Returns the ID of the event's type.
     *
     * @return The ID of T.
     */
    EventTypeId GetTypeId() const final { return EventTypeIdOf<T>(); }
};

} // namespace svanes
