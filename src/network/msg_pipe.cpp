#include <svanes/network/msg_pipe.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace svanes {

bool MsgPipe::Send(const NetworkMessage &message) {
    bool accepted = !routes.empty();
    for (const ConnectionId connection : Connections()) {
        if (!Send(connection, message)) {
            accepted = false;
        }
    }
    return accepted;
}

bool MsgPipe::Receive(NetworkMessage &message) {
    ReceivedMessage received;
    if (!Receive(received)) {
        return false;
    }
    message = std::move(received.message);
    return true;
}

std::vector<ConnectionId> MsgPipe::Connections() const {
    std::vector<ConnectionId> connections;
    connections.reserve(routes.size());
    for (std::size_t index = 0; index < routes.size(); ++index) {
        connections.push_back({static_cast<std::uint32_t>(index + 1)});
    }
    return connections;
}

ConnectionId MsgPipe::RememberConnection(const std::string &route) {
    const auto found = std::find(routes.begin(), routes.end(), route);
    if (found != routes.end()) {
        return {static_cast<std::uint32_t>(found - routes.begin() + 1)};
    }
    if (routes.size() == std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error("MsgPipe exhausted connection identifiers.");
    }
    // Handles start at one, leaving zero as an invalid connection id.
    routes.push_back(route);
    return {static_cast<std::uint32_t>(routes.size())};
}

const std::string &MsgPipe::Route(ConnectionId connection) const {
    if (connection.value == 0 || connection.value > routes.size()) {
        throw std::invalid_argument("MsgPipe: unknown connection identifier.");
    }
    return routes[connection.value - 1];
}

}
