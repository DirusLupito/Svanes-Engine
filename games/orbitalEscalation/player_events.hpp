#pragma once

#include <svanes/events/event.hpp>

#include <cstdint>
#include <string>

/**
 * Represents a player spawning in the game.
 */
class PlayerSpawnEvent final : public svanes::Event {
public:
    /**
     * Constructs an event identifying the player who is spawning.
     *
     * @param player The server assigned player ID.
     */
    explicit PlayerSpawnEvent(std::uint32_t player);

    /**
     * Returns the identifier used to subscribe to player spawn events.
     *
     * @return The event type identifier shared by all player spawn events.
     */
    static std::string GetStaticEventType();

    /**
     * Identifies this event as a player spawn so the event manager can find
     * its subscribed handlers.
     *
     * @return The event type identifier shared by all player spawn events.
     */
    const std::string GetEventType() const override;

    // The server assigned ID of the player who is spawning.
    std::uint32_t player;
};

/**
 * Represents a player's death in the game.
 */
class PlayerDeathEvent final : public svanes::Event {
public:
    /**
     * Constructs an event identifying the player who died.
     *
     * @param player The server assigned player ID.
     */
    explicit PlayerDeathEvent(std::uint32_t player);

    /**
     * Returns the identifier used to subscribe to player death events.
     *
     * @return The event type identifier shared by all player death events.
     */
    static std::string GetStaticEventType();

    /**
     * Identifies this event as a player death so the event manager can find
     * its subscribed handlers.
     *
     * @return The event type identifier shared by all player death events.
     */
    const std::string GetEventType() const override;

    // The server assigned ID of the player who died.
    std::uint32_t player;
};
