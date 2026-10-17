#include <svanes/network/clientserver/network_server.hpp>

#include <svanes/async/async_parallel_for_driver.hpp>
#include <svanes/network/clientserver/IServer.hpp>
#include <svanes/timeline_system.hpp>

#include "../msg_pipe_factory.hpp"
#include "protocol.hpp"

#include <chrono>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>

namespace svanes {

// We need only use the default constructor and destructor, as all members
// can be default-constructed and destructed.

NetworkServer::NetworkServer() = default;
NetworkServer::~NetworkServer() = default;

void NetworkServer::Listen(const std::string &receive_address) {
    if (receiver) {
        throw std::logic_error("Network server is already listening.");
    }

    auto new_receiver = internal::MakePipe();
    new_receiver->Listen(receive_address);
    receiver = std::move(new_receiver);
}

void NetworkServer::Send(ClientId client, const NetworkMessage &message) {
    if (client == 0 || client > sessions.size() ||
        !sessions[client - 1].accepted) {
        throw std::out_of_range("Network server: unknown client ID.");
    }

    sessions[client - 1].outgoing.push_back(
        internal::EncodeGameMessage(client, message));
}

void NetworkServer::Broadcast(const NetworkMessage &message) {
    for (std::size_t index = 0; index < sessions.size(); ++index) {
        if (sessions[index].accepted) {
            sessions[index].outgoing.push_back(internal::EncodeGameMessage(
                static_cast<ClientId>(index + 1), message));
        }
    }
}

std::span<const ClientInput> NetworkServer::PollInputs() const {
    return inputs;
}

void NetworkServer::ReceiveMessages(IServer &server,
                                    std::vector<ClientInput> &pending) {
    if (!receiver) {
        return;
    }

    // Buffer to hold the incoming message while we read it.
    // We only read one message at a time, so we can reuse the same object for
    // each message.
    NetworkMessage message;

    // Poll for messages until there are no more to receive.
    // In theory, if this loop is extremely slow, and a client
    // sends a lot of messages, we can get stuck in this loop forever.
    // TODO: Consider adding a timeout or a maximum number of messages to
    // process per tick.
    while (receiver->Receive(message)) {
        if (message.bytes.empty()) {
            continue;
        }

        MessageReader reader(message);

        // What we do next depends on the kind of message.

        // The first byte tells us what kind of engine message this is. See
        // ClientServerMessageKind for the possible values.
        const auto kind =
            static_cast<internal::ClientServerMessageKind>(reader.ReadUint8());

        // Case 1: A client is trying to join the server.
        // We expect the following format, read in order:
        // - 1 byte: ClientServerMessageKind::Join
        // - 2 bytes: length of the reply address = N
        // - N bytes: the reply address string
        // - remaining bytes: the join request payload

        if (kind == internal::ClientServerMessageKind::Join) {
            if (sessions.size() >= std::numeric_limits<ClientId>::max()) {
                throw std::overflow_error(
                    "Network server exhausted client IDs.");
            }

            // Address specified by the client to receive the server's response.
            // The client must be listening on this address.

            const auto reply_address = reader.ReadText();

            // The rest of the message is the join request payload, which we
            // pass to the server's HandleJoinRequest method. Meaningless to the
            // engine.
            const auto request = internal::ReadPayload(reader);

            // Message pipe we will use to send messages to this client.
            auto sender = internal::MakePipe();

            sender->Connect(reply_address);

            const auto id = static_cast<ClientId>(sessions.size() + 1);

            auto response =
                server.HandleJoinRequest({world, *this, id, request});

            sessions.push_back({std::move(sender),
                                {internal::EncodeJoinResult(
                                    response.accepted, id, response.message)},
                                response.accepted});

            // Case 2: A client has been accepted and is sending gameplay
            // messages. We expect the following format, read in order:
            // - 1 byte: ClientServerMessageKind::Game
            // - 4 bytes: the client ID
            // - remaining bytes: the gameplay message payload

        } else if (kind == internal::ClientServerMessageKind::Game) {
            const auto id = reader.ReadUint32();
            if (id != 0 && id <= sessions.size() && sessions[id - 1].accepted) {
                pending.push_back({id, internal::ReadPayload(reader)});
            }
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

        ReceiveMessages(server, pending);


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

            inputs.clear();
            pending_tics %= DefaultPhysicsStepTics;
        }

        // Send whatever messages may have been queued during the update to
        // keep clients in sync with the server's state.
        SendMessages();

        if (!server.ShouldQuit()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    return 0;
}

} // namespace svanes
