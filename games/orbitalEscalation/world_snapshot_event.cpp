#include "world_snapshot_event.hpp"

#include <utility>

WorldSnapshotEvent::WorldSnapshotEvent(WorldUpdate snapshot)
    : snapshot(std::move(snapshot)) {}
