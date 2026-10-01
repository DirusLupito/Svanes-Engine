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
    explicit NetworkServer(std::vector<PipeFactory> pipe_factories);
    ~NetworkServer();

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
     *
     * @return The queued messages with their sources, removing them from
     * the receive queue.
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
    void RunNetworkThread(Session &session, std::stop_token stop);

    std::mutex mutex;
    std::deque<ServerMessage> inbound_messages;
    std::vector<std::unique_ptr<Session>> sessions;
};

} // namespace svanes
