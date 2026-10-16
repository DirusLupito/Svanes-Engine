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

/**
 * Creates a new message pipe instance. Currently, the client-server
 * netcode is hardcoded to use UDP for message transport. In the future,
 * this function can be easily changed to create some other type of message
 * pipe, such as TCP, without requiring changes to the rest of the server code.
 *
 * @return A unique pointer to a new MsgPipe instance.
 */
std::unique_ptr<MsgPipe> MakePipe() { return std::make_unique<UdpMsgPipe>(); }

} // namespace

// We need only use the default constructor and destructor, as all members
// can be default-constructed and destructed.

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

        // Receive all available messages from the client and store them in the
        // pending vector. Each message is associated with the client ID, which
        // is index + 1 (since client IDs start from 1).

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

        // No double-sending of messages
        session.outgoing.clear();
    }
}

std::int32_t NetworkServer::run(IServer &server) {

    // First time setup.

    Vector2D gravity{};
    ServerContext context{world, *this, gravity};

    // A vector to hold pending client inputs received during the current update
    // tick. Will be swapped with the main inputs vector when processing
    // updates, then cleared to hold the next batch of pending inputs. Do not
    // use in place of the main inputs vector when processing updates.
    std::vector<ClientInput> pending;

    // Custom initialization of the server. Implemented by the user of the
    // engine. Should be called before anything reads the ServerContext.

    server.Initialize(context);

    AsyncParallelForDriver parallel_for(context.concurrency);

    // Send whatever first time messages may have been queued during
    // initialization
    SendMessages();

    // fixed origin for measuring elapsed time, so truncation to whole
    // microseconds does not discard part of a microsecond on every frame.
    const auto start_time = std::chrono::steady_clock::now();

    // The previous time in tics, used to calculate the delta time for each
    // loop iteration.
    TicCount previous_tics = 0;

    // Real tics accumulated toward the next simulation step. After a step,
    // only the fractional step remains. Overdue whole steps are lost.
    TicCount pending_tics = 0;

    while (!server.ShouldQuit()) {


        //
        // CLIENT INPUT POLLING
        //

        ReceiveInputs(pending);


        //
        // DELTA TIME CALCULATION
        //

        const auto current_tics = static_cast<TicCount>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start_time)
                .count());

        pending_tics += current_tics - previous_tics;
        previous_tics = current_tics;

        //
        // SERVER STATE UPDATE
        //

        // Suppose our simulation step is 10000 source tics, and this frame
        // accumulated 16000 source tics. We advance one complete simulation
        // step and retain the remaining 6000. Advancing a smaller step would
        // make the simulation depend on iteration time. Another 4000 real
        // tics will make the next step due. If we accumulated 35000 instead,
        // we still run only one step and retain 5000, discarding 20000 overdue
        // tics. Keeping the fraction preserves the nominal average rate when
        // the loop runs frequently enough. Discarding whole intervals prevents
        // slow iterations from scheduling more work and causing even slower
        // iterations. One expensive step can still delay updates on this same
        // thread.
        //
        // Each simulation step advances the shared clock by physics_step_tics.
        // Timelines convert that source time delta through their parents and
        // their own rates. A double speed entity receives 20000 local tics per
        // step, while a half speed entity receives 5000. Both participate in
        // the same number of simulation steps. ONLY their local elapsed time
        // differ. Normal rate timelines follow completed simulation time,
        // not real time. Discarded steps never advance the simulation.
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

            // Send whatever messages may have been queued during the update to
            // keep clients in sync with the server's state. 
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
