#include "goose_events.hpp"

#include <svanes/events/event_manager.hpp>
#include <svanes/registry.hpp>
#include <svanes/timeline_system.hpp>

#include <utility>

GooseInputEvent::GooseInputEvent(svanes::ClientId client, PlayerInput input)
    : client(client), input(std::move(input)) {}

DeathEvent::DeathEvent(svanes::Entity entity, DeathReport report)
    : entity(entity), report(report) {}

SpawnEvent::SpawnEvent(svanes::Entity entity, SpawnReport report)
    : entity(entity), report(report) {}

void QueueGooseEvent(svanes::EventManager &events, const svanes::Registry &world,
                     svanes::Entity clock, std::unique_ptr<svanes::Event> event) {
    event->priority =
        world.GetComponent<svanes::Timeline>(clock).GetTotalTics();
    events.QueueEvent(std::move(event));
}
