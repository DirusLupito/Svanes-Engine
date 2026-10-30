#include "world_snapshot_event.hpp"

#include <utility>

WorldSnapshotEvent::WorldSnapshotEvent(WorldUpdate snapshot)
    : snapshot(std::move(snapshot)) {}

std::string WorldSnapshotEvent::GetStaticEventType() {
    return "orbital.world.snapshot";
}

const std::string WorldSnapshotEvent::GetEventType() const {
    return GetStaticEventType();
}
