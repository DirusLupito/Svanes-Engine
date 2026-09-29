#pragma once

#include <svanes/network/network_session.hpp>

#include <cstdint>

/** The session id every copy of the game uses. */
inline constexpr svanes::SessionId GooseSession = 1;

/** The port a new world listens on unless another is chosen. */
inline constexpr std::uint16_t GooseHostPort = 45000;

/** The port a joining process listens on unless another is chosen. */
inline constexpr std::uint16_t GooseJoinPort = 45001;

/**
 * Message types owned by Erik's game.
 * - Input: Carries a player's input for a simulation tick.
 * - StateHash: Reports a state hash at a confirmed tick boundary.
 */
enum class GooseMessageType : svanes::MessageType {
    Input = 2,
    StateHash = 3
};

/**
 * @param type A game message type.
 * @return The same type as the session identifies it.
 */
constexpr svanes::MessageType ToMessageType(GooseMessageType type)
{
    return static_cast<svanes::MessageType>(type);
}
