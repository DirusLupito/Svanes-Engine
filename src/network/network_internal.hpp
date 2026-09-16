#pragma once

#include <svanes/network/network_message.hpp>

#include <zmq.hpp>

#include <vector>

namespace svanes::internal {

/**
 * Internal interface for pulling messages from sockets.
 * Gives access to DrainMessages to both NetworkServer and NetworkClient.
 */
class NetworkInternal final {
public:
    /**
     * Pulls from a socket and returns all retrieved messages.
     * @param socket the socket to be pulled from
     */
    static std::vector<NetworkMessage> DrainMessages(zmq::socket_t &socket);
};

} // namespace svanes::internal
