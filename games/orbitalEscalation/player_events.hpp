#pragma once

#include <svanes/events/event.hpp>

#include <cstdint>
#include <string_view>

/**
 * Represents a player spawning in the game.
 */
class PlayerSpawnEvent final : public svanes::EventOf<PlayerSpawnEvent> {
public:
    // The unique name identifying player spawn events.
    static constexpr std::string_view EventName = "orbital.player.spawn";

    /**
     * Constructs an event identifying the player who is spawning.
     *
     * @param player The server assigned player ID.
     */
    explicit PlayerSpawnEvent(std::uint32_t player);

    // The server assigned ID of the player who is spawning.
    std::uint32_t player;
};

/**
 * Represents a player's death in the game.
 */
class PlayerDeathEvent final : public svanes::EventOf<PlayerDeathEvent> {
public:
    // The unique name identifying player death events.
    static constexpr std::string_view EventName = "orbital.player.death";

    /**
     * Constructs an event identifying the player who died.
     *
     * @param player The server assigned player ID.
     */
    explicit PlayerDeathEvent(std::uint32_t player);

    // The server assigned ID of the player who died.
    std::uint32_t player;
};
