#pragma once

#include "pieces/Piece.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace ghqcxx {

/**
 * Assigns a team by the order in which clients join the server.
 *
 * @param clientId The server's one-based client ID.
 *
 * @return Blue for the first client, Red for the second, or no team for a
 * spectator.
 */
std::optional<Team> GetClientTeam(std::uint32_t clientId);

/**
 * Reports the name of a client's role.
 *
 * @param team The assigned team, or no team for a spectator.
 *
 * @return The role as displayed to the player.
 */
std::string_view GetRoleName(std::optional<Team> team);

} // namespace ghqcxx
