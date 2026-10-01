#pragma once

#include <svanes/network/msg_pipe.hpp>
#include <zmq.hpp>

#include <memory>
#include <optional>

namespace svanes {

class NetworkServer;

/**
 * A connection assigned to a joining client for gameplay messages.
 *
 * FIELDS
 * - id The client identity assigned by the server.
 * - pipe The connection used to exchange gameplay messages.
 */
struct TcpConnection {
    ClientId id;
    std::unique_ptr<MsgPipe> pipe;
};

/**
 * Requests a dedicated gameplay connection through a server's joining port.
 */
class TcpClientConnector {
public:
    /**
     * Starts joining a server without waiting for its reply.
     *
     * @param host The host running the server.
     * @param joining_port The TCP port accepting join requests.
     * @throws std::runtime_error if the join request cannot be sent.
     * @throws zmq::error_t if the connection request fails.
     */
    TcpClientConnector(std::string host, std::uint16_t joining_port);

    /**
     * Completes the join when the server's assignment arrives.
     *
     * @return The assigned identity and connection, or std::nullopt while
     * waiting. Stop polling after receiving a connection.
     * @throws std::invalid_argument if the assignment is truncated or contains
     * an invalid ID or port.
     * @throws zmq::error_t if receiving or establishing the connection fails.
     */
    std::optional<TcpConnection> Poll();

private:
    // The server host used for the assigned gameplay connection.
    std::string host;

    // ZeroMQ context used to create the request socket.
    zmq::context_t context;

    // REQ socket used to send the join request and receive the assignment.
    zmq::socket_t socket;
};

/**
 * Lets clients join through one shared port before using dedicated
 * connections for gameplay.
 */
class TcpServerListener {
public:
    /**
     * Opens the shared port used to admit new clients.
     *
     * @param joining_port The local TCP port accepting join requests.
     * @throws zmq::error_t if the joining port cannot be opened.
     */
    explicit TcpServerListener(std::uint16_t joining_port);
    /**
     * Admits waiting clients and tells each one how to reach its connection.
     *
     * @param server Owns the admitted connections and their client identities.
     * @throws std::overflow_error if the server has no client IDs left.
     * @throws std::runtime_error if a connection assignment cannot be sent.
     * @throws std::system_error if a client's network thread cannot be started.
     * @throws zmq::error_t if receiving, replying or opening a connection
     * fails.
     */
    void Poll(NetworkServer &server);

private:
    // ZeroMQ context used to create the reply socket.
    zmq::context_t context;

    // REP socket used to receive join requests and send connection assignments.
    zmq::socket_t socket;
};

} // namespace svanes
