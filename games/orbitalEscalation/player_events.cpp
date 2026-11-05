#include "player_events.hpp"

PlayerSpawnEvent::PlayerSpawnEvent(std::uint32_t player) : player(player) {}

PlayerDeathEvent::PlayerDeathEvent(std::uint32_t player) : player(player) {}
