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
    std::deque<NetworkMessage> inbound_messages;
    std::vector<std::unique_ptr<Session>> sessions;
};

} // namespace svanes
