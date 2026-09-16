#pragma once

#include <svanes/network/network_message.hpp>

#include <zmq.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace svanes {

/**
 * Class allowing a game to create a server in a client-server network.
 */
class NetworkServer final {
public:
    /**
     * Constructs a NetworkServer with the specified ports.
     * @param broadcast_port Number of the port for the socket that will broadcast messages out to clients.
     * (Might change this name later it a bit confusing I feel)
     * @param inbound_port Number of the port for the socket that will receive messages from clients.
     */
    NetworkServer(std::uint16_t broadcast_port, std::uint16_t inbound_port);

    /**
     * Public entry point for SendRaw. Ensures that message type can be interpreted as raw bytes.
     * Additionally isolates zmq calls from game code.
     * @param message The contents to be broadcast to clients.
     */
    template <typename T> void Broadcast(const T &message) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "NetworkServer::Broadcast requires a trivially copyable type.");
        SendRaw(&message, sizeof(T));
    }

    /**
     * Pulls messages from the inbound socket.
     */
    std::vector<NetworkMessage> PollInbound();

private:
    /**
     * Sends out a message through the broadcast socket to clientsx as raw bytes.
     * @param data The message passed in by Broadcast()
     * @param size Size of this message in bytes, to ensure message integrity.
     */
    void SendRaw(const void *data, std::size_t size);

    zmq::context_t context;
    zmq::socket_t broadcast_socket;
    zmq::socket_t inbound_socket;
};

} // namespace svanes
