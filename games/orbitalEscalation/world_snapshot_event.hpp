#pragma once

#include "serialization/world_update.hpp"

#include <svanes/events/event.hpp>

#include <string>

/**
 * Represents the server's complete world state received by a client. Owns the
 * decoded snapshot so it remains available until the event is handled.
 */
class WorldSnapshotEvent final : public svanes::Event {
public:
    /**
     * Constructs an event containing a world snapshot.
     *
     * @param snapshot The world state received from the server.
     */
    explicit WorldSnapshotEvent(WorldUpdate snapshot);

    /**
     * Returns the identifier used to subscribe to world snapshot events.
     *
     * @return The event type identifier shared by all world snapshot events.
     */
    static std::string GetStaticEventType();

    /**
     * Identifies this event as a world snapshot so the event manager can find
     * its subscribed handlers.
     *
     * @return The event type identifier shared by all world snapshot events.
     */
    const std::string GetEventType() const override;

    // The complete world state received from the server.
    WorldUpdate snapshot;
};
