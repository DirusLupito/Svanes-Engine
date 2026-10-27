// Inspired by code from https://denyskryvytskyi.github.io/event-system

#pragma once

#include "abstract_event_handler_wrapper.hpp"

#include <concepts>
#include <functional>
#include <string>
#include <typeinfo>

namespace svanes {

// An event handler is a function that takes an event of a specific type and
// performs some action in response to that event. Returning nothing.
template <std::derived_from<Event> EventType>
using EventHandler = std::function<void(const EventType &)>;

/**
 * A wrapper class for event handlers that allows them to be stored and executed
 * in a type-erased manner. This class inherits from AbstractEventHandlerWrapper
 * and provides a concrete implementation for handling events of a specific
 * type.
 *
 * @tparam EventType The type of event that this handler can handle. It must be
 * a subclass of Event and provide a static GetStaticEventType() method that
 * returns the same identifier as GetEventType().
 */
template <std::derived_from<Event> EventType>
class EventHandlerWrapper : public AbstractEventHandlerWrapper {
public:
    /**
     * Constructs an EventHandlerWrapper with the given event handler callback.
     *
     * @param callback The event handler function to be wrapped. It should be a
     * function that takes a const reference to an EventType and returns void.
     */
    explicit EventHandlerWrapper(const EventHandler<EventType> &callback)
        : handler(callback), handler_type(handler.target_type().name()) {}

    /**
     * Returns the type name of the wrapped callback.
     *
     * @return A string representing the wrapped callback's type.
     */
    std::string GetType() const override { return handler_type; }

private:
    /**
     * Executes the wrapped event handler if the event type matches the expected
     * EventType. This method is called by the Exec method of the base class.
     *
     * @param event The event to be handled. It is checked against the expected
     * EventType, and if it matches, the handler is invoked.
     */
    void Call(const Event &event) override {
        if (event.GetEventType() == EventType::GetStaticEventType()) {
            handler(static_cast<const EventType &>(event));
        }
    }

    // The actual event handler function that will be called when an event of
    // the appropriate type is received.
    EventHandler<EventType> handler;

    // The type name of the wrapped callback, used for identification purposes.
    const std::string handler_type;
};

} // namespace svanes
