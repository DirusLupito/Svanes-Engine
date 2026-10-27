// Inspired by code from https://denyskryvytskyi.github.io/event-system

#pragma once

#include "event.hpp"

#include <string>

namespace svanes {

/**
 * An abstract base class for event handler wrappers.
 * This class provides a common interface for all event handler wrappers.
 */
class AbstractEventHandlerWrapper {
public:
    /**
     * Default destructor for the AbstractEventHandlerWrapper class.
     */
    virtual ~AbstractEventHandlerWrapper() = default;

    /**
     * Executes the event handler for the given event.
     * This method calls the derived class's implementation of the Call method.
     *
     * @param event The event to be handled.
     */
    void Exec(const Event &event) { Call(event); }

    /**
     * Returns the type name of the wrapped callback.
     * Derived classes must provide this name to identify callback types.
     *
     * @return A string representing the wrapped callback's type.
     */
    virtual std::string GetType() const = 0;

private:
    /**
     * Callback method to be implemented by derived classes to handle the event.
     *
     * @param event The event to be handled.
     */
    virtual void Call(const Event &event) = 0;
};

} // namespace svanes
