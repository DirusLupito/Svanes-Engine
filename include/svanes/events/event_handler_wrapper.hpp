// Inspired by code from https://denyskryvytskyi.github.io/event-system

#pragma once

#include <svanes/events/abstract_event_handler_wrapper.hpp>

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
 * @tparam EventType The type of event that this handler can handle. It must
 * derive from EventOf<EventType>, which requires it to declare an EventName.
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

    /**
     * Returns the ID of EventType, which the event manager registers this
     * handler under.
     *
     * @return The ID of EventType.
     */
    EventTypeId GetEventTypeId() const override {
        return EventTypeIdOf<EventType>();
    }

private:
    /**
     * Executes the wrapped event handler. This method is called by the Exec
     * method of the base class.
     *
     * @param event The event to be handled. It must be an EventType, which the
     * event manager guarantees by looking up handlers using the event's ID.
     * This is not checked here.
     */
    void Call(const Event &event) override {
        handler(static_cast<const EventType &>(event));
    }

    // The actual event handler function that will be called when an event of
    // the appropriate type is received.
    EventHandler<EventType> handler;

    // The type name of the wrapped callback, used for identification purposes.
    const std::string handler_type;
};

} // namespace svanes
