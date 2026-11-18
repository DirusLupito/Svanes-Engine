#pragma once

#include <svanes/network/network_message.hpp>

#include <cstdint>

namespace ghqcxx {

/**
 * Identifies the contents of a GHQ gameplay message.
 *
 * MEMBERS:
 * - Movement: A request or notification to move a piece.
 * - BoardState: The server's authoritative board.
 * - Placement: A request or notification to place a piece from reserve.
 */
enum class GameMessageType : std::uint8_t { Movement, BoardState, Placement };

/**
 * Reads the type identifying a gameplay message.
 *
 * @param message The message to inspect.
 *
 * @return The type of its contents.
 *
 * @throws std::invalid_argument if the message is empty or its type is unknown.
 */
GameMessageType GetGameMessageType(const svanes::NetworkMessage &message);

} // namespace ghqcxx
