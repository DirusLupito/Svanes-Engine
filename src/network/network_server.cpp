#include <svanes/network/network_server.hpp>

#include <svanes/async/async_parallel_for_driver.hpp>
#include <svanes/network/IServer.hpp>
#include <svanes/network/tcp_msg_pipe.hpp>
#include <svanes/network/udp_msg_pipe.hpp>
#include <svanes/timeline_system.hpp>

#include <chrono>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>

namespace svanes {

namespace {

std::unique_ptr<MsgPipe> MakePipe() { return std::make_unique<UdpMsgPipe>(); }
}

NetworkServer::NetworkServer() = default;
NetworkServer::~NetworkServer() = default;

ClientId NetworkServer::AddSession(const std::string &receive_address,
                                   const std::string &client_receive_address) {
    if (sessions.size() >= std::numeric_limits<ClientId>::max()) {
        throw std::overflow_error("Network server exhausted client IDs.");
    }
    for (const Session &session : sessions) {
        if (session.receive_address == receive_address) {
            throw std::invalid_argument(
                "Network server: receiving address is already registered.");
        }
    }

    auto receiver = MakePipe();
    receiver->Listen(receive_address);
    auto sender = MakePipe();
    sender->Connect(client_receive_address);
    sessions.push_back(
        {receive_address, std::move(receiver), std::move(sender), {}});
    return static_cast<ClientId>(sessions.size());
}

void NetworkServer::Send(ClientId client, const NetworkMessage &message) {
    if (client == 0 || client > sessions.size()) {
        throw std::out_of_range("Network server: unknown client ID.");
    }
    sessions[client - 1].outgoing.push_back(message);
}

void NetworkServer::Broadcast(const NetworkMessage &message) {
    for (Session &session : sessions) {
        session.outgoing.push_back(message);
    }
}

std::span<const ClientInput> NetworkServer::PollInputs() const {
    return inputs;
}

void NetworkServer::ReceiveInputs(std::vector<ClientInput> &pending) {
    for (std::size_t index = 0; index < sessions.size(); ++index) {
        NetworkMessage message;
        while (sessions[index].receiver->Receive(message)) {
            pending.push_back(
                {static_cast<ClientId>(index + 1), std::move(message)});
        }
    }
}

void NetworkServer::SendMessages() {
    for (Session &session : sessions) {
        for (const NetworkMessage &message : session.outgoing) {
            if (!session.sender->Send(message)) {
                throw std::runtime_error(
                    "Network server could not send a message.");
            }
        }
        session.outgoing.clear();
    }
}

std::int32_t NetworkServer::run(IServer &server) {
    Vector2D gravity{};
    ServerContext context{world, *this, gravity};
    server.Initialize(context);
    AsyncParallelForDriver parallel_for(context.concurrency);
    SendMessages();

    std::vector<ClientInput> pending;
    const auto start = std::chrono::steady_clock::now();
    TicCount previous_tics = 0;
    TicCount pending_tics = 0;

    while (!server.ShouldQuit()) {
        ReceiveInputs(pending);

        const auto current_tics = static_cast<TicCount>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start)
                .count());
        pending_tics += current_tics - previous_tics;
        previous_tics = current_tics;

        if (pending_tics >= DefaultPhysicsStepTics) {
            inputs.swap(pending);
            pending.clear();

            server.Update({world, *this, gravity});
            AdvanceTimelines(world, DefaultPhysicsStepTics);
            AdvancePhysics(world, gravity, parallel_for,
                           [&](std::span<const PhysicsTimeStep> steps) {
                               server.PhysicsUpdate(
                                   {world, *this, steps, parallel_for});
                           });
            SendMessages();
            inputs.clear();
            pending_tics %= DefaultPhysicsStepTics;
        }

        if (!server.ShouldQuit()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    return 0;
}

} // namespace svanes
