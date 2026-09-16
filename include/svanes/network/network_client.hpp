#pragma once

#include <svanes/network/network_message.hpp>

#include <zmq.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace svanes {

/**
 * Class allowing a game to create a client in a client-server network.
 */
class NetworkClient final {
public:
    /**
     * Constructs a NetworkClient with the specified host and ports.
     * @param host Hostname of the game server.
     * @param broadcast_port Number of the port for the socket that will listen for server broadcasts.
     * (Might change this name later it a bit confusing I feel)
     * @param outbound_port Number of the port for the socket that will send information to the server.
     */
    NetworkClient(const std::string &host, std::uint16_t broadcast_port, std::uint16_t outbound_port);

    /**
     * Each client has a unique ID to allow the server to correctly communicate with multiple clients simultaneously
     */
    ClientId Id() const;

    /**
     * Public entry point for SendRaw. Ensures that message type can be interpreted as raw bytes.
     * Additionally isolates zmq calls from game code.
     * @param message The contents to be sent to the server.
     */
    template <typename T> void Send(const T &message) {
        static_assert(std::is_trivially_copyable_v<T>, "NetworkClient::Send requires a trivially copyable type.");
        SendRaw(&message, sizeof(T));
    }

    /**
     * Pulls messages from the broadcast socket.
     */
    std::vector<NetworkMessage> PollBroadcast();

private:
    /**
     * Sends out a message through the outbound socket to the server as raw bytes.
     * @param data The message passed in by Send()
     * @param size Size of this message in bytes, to ensure message integrity.
     */
    void SendRaw(const void *data, std::size_t size);

    ClientId id;
    zmq::context_t context;
    zmq::socket_t broadcast_socket;
    zmq::socket_t outbound_socket;
};

} // namespace svanes
