#include <svanes/events/event_manager.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace svanes {

void EventManager::Shutdown() {
    if (active_triggers != 0 || dispatching) {
        throw std::logic_error("Cannot shut down during event delivery.");
    }

    events_queue.clear();
    subscribers.clear();
}

void EventManager::Subscribe(
    const std::string &event_id,
    std::unique_ptr<AbstractEventHandlerWrapper> &&handler) {
    if (active_triggers != 0) {
        throw std::logic_error("Cannot subscribe during an event callback.");
    }

    if (event_id.empty()) {
        throw std::invalid_argument("Event identifier cannot be empty.");
    }

    if (!handler) {
        throw std::invalid_argument("Event handler cannot be null.");
    }

    auto &handlers = subscribers[event_id];

    // iterate through existing handlers for this event type to check for duplicates
    for (const auto &existing : handlers) {
        if (existing->GetType() == handler->GetType()) {
            throw std::invalid_argument("Event handler is already registered.");
        }
    }

    handlers.push_back(std::move(handler));
}

void EventManager::Unsubscribe(const std::string &event_id,
                               const std::string handler_name) {
    if (active_triggers != 0) {
        throw std::logic_error("Cannot unsubscribe during an event callback.");
    }

    // Find the handlers for the given event_id
    const auto found = subscribers.find(event_id);
    if (found != subscribers.end()) {
        auto &handlers = found->second;

        // Iterate through the handlers to find the one which matches the given handler_name
        for (auto entry = handlers.begin(); entry != handlers.end(); ++entry) {
            if ((*entry)->GetType() == handler_name) {
                handlers.erase(entry);
                if (handlers.empty()) {
                    subscribers.erase(found);
                }

                // Successfully removed the handler, so return early.
                return;
            }
        }
    }

    // If we reach here, the handler was not found for the given event_id.
    throw std::invalid_argument(
        "Event handler is not registered for this event.");
}

void EventManager::TriggerEvent(const Event &event) {

    // find the handlers subscribed to the event's type
    const auto found = subscribers.find(event.GetEventType());
    if (found == subscribers.end()) {
        // noone can handle this event, nothing to do.
        return;
    }

    ++active_triggers;
    try {
        // Call each handler in registration order, passing the event to it.
        for (const auto &handler : found->second) {
            handler->Exec(event);
        }
    } catch (...) {
        --active_triggers;
        throw;
    }
    --active_triggers;
}

void EventManager::QueueEvent(std::unique_ptr<Event> &&event) {
    if (!event) {
        throw std::invalid_argument("Queued event cannot be null.");
    }

    events_queue.push_back(std::move(event));
}

void EventManager::DispatchEvents() {
    DispatchEvents(std::numeric_limits<EventPriority>::max());
}

void EventManager::DispatchEvents(EventPriority maximum_priority) {

    // check if anyone is currently processing events, or if we are already dispatching
    if (active_triggers != 0 || dispatching) {
        throw std::logic_error(
            "Cannot dispatch queued events during delivery.");
    }

    // Limit the pass to existing events so callbacks cannot keep it running
    // indefinitely by producing more events.
    const auto count = events_queue.size();
    std::size_t consumed = 0;
    dispatching = true;
    try {
        // Events are processed in ascending priority order, with insertion order
        // used to break ties.
        std::stable_sort(events_queue.begin(), events_queue.begin() + count,
                         [](const auto &left, const auto &right) {
                             return left->priority < right->priority;
                         });

        while (consumed < count &&
               events_queue[consumed]->priority <= maximum_priority) {
            // Move ownership into a local variable rather than keeping a
            // reference to a queue slot that callbacks could invalidate by
            // adding events.
            auto event = std::move(events_queue[consumed++]);
            if (!event->Handled) {
                TriggerEvent(*event);
            }
        }
    } catch (...) {
        // Remove events already processed, including the one whose handler
        // threw. Keep all undelivered events queued for the next pass.
        events_queue.erase(events_queue.begin(),
                           events_queue.begin() + consumed);
        dispatching = false;
        throw;
    }

    // Remove all events that were processed, whether they were handled or not.
    events_queue.erase(events_queue.begin(), events_queue.begin() + consumed);
    dispatching = false;
}

} // namespace svanes
