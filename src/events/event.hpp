// Inspired by code from https://denyskryvytskyi.github.io/event-system

#pragma once

#include <string>

namespace svanes {

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
     * Returns the type of the event as a string.
     * This method must be implemented by derived classes to provide the
     * specific event type.
     *
     * @return A string representing the type of the event.
     */
    virtual const std::string GetEventType() const = 0;

    /**
     * Indicates whether the event has been handled by any event handler.
     */
    bool Handled{false};
};

} // namespace svanes
