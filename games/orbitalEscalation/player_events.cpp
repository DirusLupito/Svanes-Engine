#include "player_events.hpp"

PlayerSpawnEvent::PlayerSpawnEvent(std::uint32_t player) : player(player) {}

std::string PlayerSpawnEvent::GetStaticEventType() {
    return "orbital.player.spawn";
}

const std::string PlayerSpawnEvent::GetEventType() const {
    return GetStaticEventType();
}

PlayerDeathEvent::PlayerDeathEvent(std::uint32_t player) : player(player) {}

std::string PlayerDeathEvent::GetStaticEventType() {
    return "orbital.player.death";
}

const std::string PlayerDeathEvent::GetEventType() const {
    return GetStaticEventType();
}
