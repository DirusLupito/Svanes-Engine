#pragma once

#include "serialization/world_update.hpp"

#include <svanes/events/event.hpp>

#include <string_view>

/**
 * Represents the server's complete world state received by a client. Owns the
 * decoded snapshot so it remains available until the event is handled.
 */
class WorldSnapshotEvent final : public svanes::EventOf<WorldSnapshotEvent> {
public:
    // The unique name identifying world snapshot events.
    static constexpr std::string_view EventName = "orbital.world.snapshot";

    /**
     * Constructs an event containing a world snapshot.
     *
     * @param snapshot The world state received from the server.
     */
    explicit WorldSnapshotEvent(WorldUpdate snapshot);

    // The complete world state received from the server.
    WorldUpdate snapshot;
};
