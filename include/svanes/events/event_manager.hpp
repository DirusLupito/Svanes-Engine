// Inspired by https://denyskryvytskyi.github.io/event-system

#pragma once

#include <svanes/events/abstract_event_handler_wrapper.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace svanes {

/**
 * Represents a manager for the lifecycle of events and their associated
 * handlers. This class provides functionality to subscribe, unsubscribe,
 * trigger, queue, and dispatch events, ensuring that handlers for specific
 * event types are executed in a controlled manner.
 */
class EventManager final {
public:
    /**
     * Creates an empty manager with no queued events or subscriptions.
     */
    EventManager() = default;

    /**
     * Override of the copy constructor to prevent copying of EventManager
     * instances, which own their queued events and handlers.
     *
     * This prevents something like:
     * EventManager manager1;
     * EventManager manager2 = manager1;
     */
    EventManager(const EventManager &) = delete;

    /**
     * Override of the copy assignment operator to prevent copying of
     * EventManager instances.
     *
     * This prevents something like:
     * EventManager manager1;
     * EventManager manager2;
     * manager2 = manager1;
     */
    EventManager &operator=(const EventManager &) = delete;

    /**
     * Override of the move constructor to prevent moving of EventManager
     * instances. Callbacks can refer to the manager they registered with, so
     * that manager must stay at the same address.
     *
     * This prevents something like:
     * EventManager manager1;
     * EventManager manager2 = std::move(manager1);
     */
    EventManager(EventManager &&) = delete;

    /**
     * Override of the move assignment operator to prevent moving of
     * EventManager instances.
     *
     * This prevents something like:
     * EventManager manager1;
     * EventManager manager2;
     * manager2 = std::move(manager1);
     */
    EventManager &operator=(EventManager &&) = delete;

    /**
     * Discards all pending events and unregisters every handler, releasing
     * the resources owned by this manager. The manager can be reused by
     * registering new handlers and queuing new events.
     *
     * @throws std::logic_error if called during event delivery.
     */
    void Shutdown();

    /**
     * Takes ownership of a handler for event_id and maps it to the given event
     * type identifier. The handler will be called when TriggerEvent is called
     * with an event whose event type matches the one given here.
     *
     * @param event_id The event type identifier to associate with the handler.
     * It must match the identifier returned by the events it handles.
     * @param handler The wrapper to register. Ownership transfers to the
     * manager when registration succeeds.
     *
     * @throws std::invalid_argument if event_id is empty, handler is null, or
     * a handler with the same GetType() is already registered for event_id.
     * @throws std::logic_error if called during a callback.
     */
    void Subscribe(const std::string &event_id,
                   std::unique_ptr<AbstractEventHandlerWrapper> &&handler);

    /**
     * Unregisters one callback so it will no longer receive events of the
     * specified type.
     *
     * @param event_id The event type identifier used when registering the
     * handler.
     * @param handler_name The callback type name returned by the registered
     * wrapper's GetType().
     *
     * @throws std::invalid_argument if the registration does not exist.
     * @throws std::logic_error if called during a callback.
     */
    void Unsubscribe(const std::string &event_id,
                     const std::string handler_name);

    /**
     * Finds the handlers subscribed to the given event's type and passes the
     * event to each of their callbacks. This lets the code reporting an event
     * notify interested systems without knowing which systems are listening.
     * The callbacks run immediately, in registration order, before this
     * function returns.
     *
     * @param event The event to handle.
     *
     * @throws ... Propagates any exception thrown by a registered handler.
     * If a callback throws, the remaining handlers are not called. Effects of
     * callbacks that already ran are not rolled back.
     */
    void TriggerEvent(const Event &event);

    /**
     * Stores an event so its subscribed handlers can process it later.
     * The manager owns the event until it is dispatched or discarded.
     *
     * @param event The event to enqueue.
     *
     * @throws std::invalid_argument if event is null.
     */
    void QueueEvent(std::unique_ptr<Event> &&event);

    /**
     * Processes all events queued by QueueEvent in ascending priority order.
     * Events with equal priorities use insertion order. Each event is passed
     * to every registered handler for its type, in registration order, unless
     * the event's Handled flag is set to true. Events are removed from the
     * queue after processing. Events queued by callbacks will not be processed
     * until the next call to this method.
     *
     * @throws std::logic_error if called during event delivery.
     * @throws ... Propagates any exception thrown by a registered handler.
     * The event being processed is discarded, and undelivered events remain
     * queued ahead of events added by callbacks. Effects of callbacks that
     * already ran are not rolled back.
     */
    void DispatchEvents();

    /**
     * Processes events queued by QueueEvent with priorities at or below
     * maximum_priority, in ascending priority order. Events with larger
     * priorities remain queued.
     * Events with equal priorities use insertion order. Each event is passed
     * to every registered handler for its type, in registration order, unless
     * the event's Handled flag is set to true. Events are removed from the
     * queue after processing. Events queued by callbacks will not be processed
     * until the next call to this method.
     *
     * @param maximum_priority The inclusive upper limit on priority values.
     *
     * @throws std::logic_error if called during event delivery.
     * @throws ... Propagates any exception thrown by a registered handler.
     * The event being processed is discarded, and undelivered events remain
     * queued ahead of events added by callbacks. Effects of callbacks that
     * already ran are not rolled back.
     */
    void DispatchEvents(EventPriority maximum_priority);

private:
    // Owns pending events until they are consumed or discarded by Shutdown.
    std::vector<std::unique_ptr<Event>> events_queue;

    // Maps event type identifiers to owned handlers. Each vector preserves
    // registration order so delivery order is predictable.
    std::unordered_map<
        std::string, std::vector<std::unique_ptr<AbstractEventHandlerWrapper>>>
        subscribers;

    // The number of nested TriggerEvent calls executing handlers. Registration
    // changes must wait until all of them return to avoid invalidating
    // handlers.
    std::size_t active_triggers = 0;

    // Guards the entire queue pass, including event destruction, against
    // recursive dispatch and Shutdown.
    bool dispatching = false;
};

} // namespace svanes
