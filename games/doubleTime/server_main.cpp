#include "double_time_server.hpp"

#include <svanes/network/clientserver/network_server.hpp>

#include <cstdint>
#include <stdexcept>

std::int32_t main(std::int32_t argc, char **) {
    if (argc > 1) {
        throw std::invalid_argument(
            "The Double Time server takes no arguments.");
    }
    svanes::NetworkServer network;
    DoubleTimeServer game;
    return network.run(game);
}
