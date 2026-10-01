#pragma once

#include <svanes/network/msg_pipe.hpp>
#include <svanes/network/network_message.hpp>

#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace svanes {

class TcpServerListener;

/**
 * Pairs a received message with its connection so requests and replies can
 * be associated with the correct client.
 *
 * FIELDS:
 * - session: The zero-based slot in the server's list of pipe factories.
 * - message: The content received from that connection.
 */
struct ServerMessage {
    std::size_t session;
    NetworkMessage message;
};

/**
 * A network server for handling messages between the server and clients.
 * Defines PipeFactory function to construct message pipes within the servers
 * network thread.
 * 
 * Each client connecting to the server will have its own MessagePipe in its
 * own thread.
 */
class NetworkServer final {
public:
    using PipeFactory = std::function<std::unique_ptr<MsgPipe>()>;

    explicit NetworkServer(PipeFactory pipe_factory);

    /**
     * Creates a server with its initial client connections ready for use.
     * Errors from the pipe factories propagate to the caller.
     *
     * @param pipe_factories Creates each client's pipe on its network thread.
     * @throws std::invalid_argument if the list is empty, a factory is empty,
     * or a factory returns no pipe.
     * @throws std::overflow_error if the connections exhaust client IDs.
     * @throws std::system_error if a network thread cannot be started.
     */
    explicit NetworkServer(std::vector<PipeFactory> pipe_factories);

    /**
     * Accepts new clients through a shared TCP joining port.
     *
     * @param joining_port The local TCP port for client join requests.
     * @throws zmq::error_t if the joining port cannot be opened.
     */
    explicit NetworkServer(std::uint16_t joining_port);

    ~NetworkServer();

    /**
     * Adds a client connection and waits until its pipe is ready for use.
     * Errors from the pipe factory propagate to the caller.
     *
     * @param pipe_factory Creates the client's pipe on its network thread.
     * @return The connection slot used to identify received messages and
     * address replies.
     * @throws std::invalid_argument if the factory is empty or returns no pipe.
     * @throws std::overflow_error if no more client IDs are available.
     * @throws std::system_error if the network thread cannot be started.
     */
    std::size_t AddSession(PipeFactory pipe_factory);

    /**
     * Queues an encoded message for one client connection.
     *
     * @param session The connection slot reported by ServerMessage.
     * @param message The encoded content to send.
     * @throws std::out_of_range if the session slot does not exist.
     */
    void Send(std::size_t session, const NetworkMessage &message);

    /**
     * Takes the waiting messages and their connection slots so the caller
     * can identify each sender and direct its replies.
     * Also admits waiting clients when using a TCP joining port.
     *
     * @return The queued messages with their sources, removing them from
     * the receive queue.
     * @throws std::overflow_error if admitting clients exhausts client IDs.
     * @throws std::runtime_error if a connection assignment cannot be sent.
     * @throws std::system_error if a client's network thread cannot be started.
     * @throws zmq::error_t if accepting a connection fails.
     */
    std::vector<ServerMessage> PollInboundWithSource();

    template <typename T> void Broadcast(const T &message) {
        QueueBroadcast(NetworkMessage::From(message));
    }

    std::vector<NetworkMessage> PollInbound();

/**
 * Representation of the server sesstion between a client and the server
 */
private:
    struct Session;

    void QueueBroadcast(NetworkMessage message);
    /**
     * Exchanges queued messages for one client until shutdown is requested.
     * Reports pipe startup errors to the caller admitting the connection.
     *
     * @param session The connection whose incoming and outgoing messages
     * are exchanged.
     * @param stop Requests that the connection's network loop finish.
     */
    void RunNetworkThread(Session &session, std::stop_token stop);

    // Admits clients through the shared port when dynamic joining is enabled.
    std::unique_ptr<TcpServerListener> listener;
    std::mutex mutex;
    std::deque<ServerMessage> inbound_messages;
    std::vector<std::unique_ptr<Session>> sessions;
};

} // namespace svanes
