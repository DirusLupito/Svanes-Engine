#include "ClientRole.hpp"

namespace ghqcxx {

std::optional<Team> GetClientTeam(std::uint32_t clientId) {
    switch (clientId) {
    case 1:
        return Team::Blue;
    case 2:
        return Team::Red;
    default:
        return std::nullopt;
    }
}

std::string_view GetRoleName(std::optional<Team> team) {
    if (!team) {
        return "Spectator";
    }
    return *team == Team::Blue ? "Blue" : "Red";
}

} // namespace ghqcxx
