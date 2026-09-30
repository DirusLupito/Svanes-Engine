#include <svanes/network/network_server.hpp>

#include <chrono>
#include <stdexcept>
#include <utility>

namespace svanes {

namespace {

NetworkServer::PipeFactory RequirePipeFactory(NetworkServer::PipeFactory factory) {
    if (!factory) {
        throw std::invalid_argument("Network server requires a pipe factory.");
    }
    return factory;
}

} // namespace

// Representation of one connection between a client and the server
// Owns a pipe factory to construct pipe and ensure the socket is
// fully contained in this thread
struct NetworkServer::Session {
    explicit Session(PipeFactory pipe_factory)
        : pipe_factory(std::move(pipe_factory)),
          network_thread() {}

    PipeFactory pipe_factory;
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<NetworkMessage> outbound_messages;
    std::jthread network_thread;
};

// Cunstructors for one session server
NetworkServer::NetworkServer(PipeFactory pipe_factory)
    : NetworkServer(std::vector<PipeFactory>{std::move(pipe_factory)}) {}

// Constructor for a multi session server
NetworkServer::NetworkServer(std::vector<PipeFactory> pipe_factories) {
    if (pipe_factories.empty()) {
        throw std::invalid_argument(
            "Network server requires at least one pipe factory.");
    }

    sessions.reserve(pipe_factories.size());
    for (PipeFactory &factory : pipe_factories) {
        auto session = std::make_unique<Session>(
            RequirePipeFactory(std::move(factory)));
        session->network_thread = std::jthread(
            [this, session_pointer = session.get()](std::stop_token stop) {
                RunNetworkThread(*session_pointer, stop);
            });
        sessions.push_back(std::move(session));
    }
}

NetworkServer::~NetworkServer() = default;

// Loops over every client connection to copy a message into that
// sessions outbound message queue.
void NetworkServer::QueueBroadcast(NetworkMessage message) {
    for (const std::unique_ptr<Session> &session : sessions) {
        {
            std::lock_guard lock(session->mutex);
            session->outbound_messages.push_back(message);
        }
        session->condition.notify_one();
    }
}

// Pulls in pending messages and clears the queue.
// The messages are then returned to be used by the server runtime.
std::vector<NetworkMessage> NetworkServer::PollInbound() {
    std::vector<NetworkMessage> messages;

    std::lock_guard lock(mutex);
    messages.reserve(inbound_messages.size());
    while (!inbound_messages.empty()) {
        messages.push_back(std::move(inbound_messages.front()));
        inbound_messages.pop_front();
    }

    return messages;
}

// Runs a thread containing a network pipe for a given client.
// Uses the pipe factory to create the message pipe.
// AMQ does not allow sockets to be shared accross threads, hence
// the need fo the pipe factory.
void NetworkServer::RunNetworkThread(Session &session, std::stop_token stop) {
    std::unique_ptr<MsgPipe> pipe = session.pipe_factory();
    if (!pipe) {
        throw std::invalid_argument("Network server pipe factory returned null.");
    }

    while (!stop.stop_requested()) {
        std::deque<NetworkMessage> outbound;
        {
            std::lock_guard lock(session.mutex);
            outbound.swap(session.outbound_messages);
        }

        for (const NetworkMessage &message : outbound) {
            pipe->Send(message);
        }

        ReceivedMessage received;
        while (pipe->Receive(received)) {
            std::lock_guard lock(mutex);
            inbound_messages.push_back(std::move(received.message));
            received = {};
        }

        std::unique_lock lock(session.mutex);
        session.condition.wait_for(lock, std::chrono::milliseconds(1),
                                   [&] {
                                       return stop.stop_requested() ||
                                              !session.outbound_messages.empty();
                                   });
    }
}

} // namespace svanes
