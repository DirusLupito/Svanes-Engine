#pragma once

#include <svanes/network/msg_pipe.hpp>
#include <svanes/network/network_message.hpp>

#include <memory>
#include <vector>

namespace svanes {

class TcpClientConnector;

class NetworkClient final {
public:
    explicit NetworkClient(std::unique_ptr<MsgPipe> pipe);
    /**
     * Requests a connection to a server. PollBroadcast completes the join.
     *
     * @param host The host running the server.
     * @param joining_port The server's TCP joining port.
     * @throws std::runtime_error if the join request cannot be sent.
     * @throws zmq::error_t if the connection request fails.
     */
    NetworkClient(std::string host, std::uint16_t joining_port);

    /**
     * Releases the client's connection or pending join request.
     */
    ~NetworkClient();

    /**
     * Takes ownership of another client's connection or pending join request.
     */
    NetworkClient(NetworkClient &&) noexcept;

    /**
     * Replaces this client's connection with another client's connection
     * or pending join request.
     *
     * @return This client after taking ownership.
     */
    NetworkClient &operator=(NetworkClient &&) noexcept;

    ClientId Id() const;
    /**
     * Checks whether the client is ready to send gameplay messages.
     *
     * @return True when a message pipe is available, false while joining.
     */
    bool IsConnected() const;

    /**
     * Sends an already encoded message through the client's connection.
     *
     * @param message The encoded content to send to the server.
     * @throws std::logic_error if the client is still joining.
     */
    void Send(const NetworkMessage &message);

    /**
     * Sends a value to a server that uses the same representation of its type.
     *
     * @tparam T A trivially copyable message type.
     * @param message The value to send.
     * @throws std::logic_error if the client is still joining.
     */
    template <typename T> void Send(const T &message) {
        Send(NetworkMessage::From(message));
    }

    /**
     * Completes any pending join and takes the messages waiting from the
     * server.
     *
     * @return The received messages, or an empty list while joining or when
     * no messages are waiting.
     * @throws std::invalid_argument if the connection assignment is truncated
     * or contains an invalid ID or port.
     * @throws zmq::error_t if receiving or establishing the connection fails.
     */
    std::vector<NetworkMessage> PollBroadcast();

private:
    ClientId id;
    std::unique_ptr<MsgPipe> pipe;

    // Keeps the join request alive until the server assigns a connection.
    std::unique_ptr<TcpClientConnector> connector;
};

} // namespace svanes
