#include <svanes/network/clientserver/network_server.hpp>

#include <svanes/async/async_parallel_for_driver.hpp>
#include <svanes/events/event_manager.hpp>
#include <svanes/network/clientserver/IServer.hpp>
#include <svanes/timeline_system.hpp>

#include "../msg_pipe_factory.hpp"
#include "protocol.hpp"

#include <chrono>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>

namespace svanes {

NetworkServer::NetworkServer()
    : communication_thread(
          [this](std::stop_token stop) { Communicate(stop); }) {}

NetworkServer::~NetworkServer() {
    communication_thread.request_stop();
    communication_thread.join();
}

void NetworkServer::Listen(const std::string &address) {
    std::lock_guard lock(outgoing_mutex);
    if (listen_address) {
        throw std::logic_error("Network server is already listening.");
    }

    listen_address = address;
}

void NetworkServer::Send(ClientId client, const NetworkMessage &message) {
    if (client == 0 || client > sessions.size() ||
        !sessions[client - 1].accepted) {
        throw std::out_of_range("Network server: unknown client ID.");
    }

    sessions[client - 1].outgoing.push_back(
        internal::EncodeGameMessage(message));
}

void NetworkServer::Broadcast(const NetworkMessage &message) {
    for (Session &session : sessions) {
        if (session.accepted) {
            session.outgoing.push_back(internal::EncodeGameMessage(message));
        }
    }
}

std::span<const ClientInput> NetworkServer::PollInputs() const {
    return inputs;
}

void NetworkServer::ReceiveMessages(IServer &server,
                                    std::vector<ClientInput> &pending) {
    // The batch of messages received since the main thread last collected
    // messages.
    std::vector<AddressedMessage> received;

    // Anonymous scope to limit the lifetime of the lock_guard.
    {
        std::lock_guard lock(incoming_mutex);
        received.swap(incoming);
    }

    // For each message in the batch, we handle its join request, heartbeat,
    // or disconnect notice, or collect its gameplay payload as input from an
    // admitted client.
    // The communication thread can receive more messages while we process this
    // batch, they'll be collected in the next iteration.
    for (const auto &[address, message] : received) {
        if (message.bytes.empty()) {
            continue;
        }

        // The decoded message from the raw NetworkMessage.
        auto decoded = internal::DecodeMessage(message);

        // Case 1: A client is trying to join the server.

        if (decoded.kind == internal::ClientServerMessageKind::Join) {
            if (sessions.size() >= std::numeric_limits<ClientId>::max()) {
                throw std::overflow_error(
                    "Network server exhausted client IDs.");
            }

            // An address can only identify one session. So we ignore duplicate
            // join requests so the existing session remains unchanged.
            if (clients.contains(address)) {
                std::cerr << "Network server: ignoring join request from an "
                             "address that already has a session: "
                          << address << '\n';
                continue;
            }

            const auto id = static_cast<ClientId>(sessions.size() + 1);

            // The game's join request payload is passed to HandleJoinRequest
            // so the game can decide whether to admit this client.
            auto response =
                server.HandleJoinRequest({world, *this, id, decoded.payload});

            sessions.push_back({address,
                                {internal::EncodeJoinResult(
                                    response.accepted, id, response.message)},
                                response.accepted,
                                std::chrono::steady_clock::now()});

            clients.emplace(address, id);

            // Case 2: An accepted client sent gameplay or a heartbeat.

        } else if (decoded.kind == internal::ClientServerMessageKind::Game ||
                   decoded.kind ==
                       internal::ClientServerMessageKind::Heartbeat) {
            const auto id = decoded.client_id;
            if (id != 0 && id <= sessions.size() && sessions[id - 1].accepted) {
                // The client's latest source address is where replies should
                // go, even if its socket has changed ports since joining.
                sessions[id - 1].address = address;
                sessions[id - 1].last_received =
                    std::chrono::steady_clock::now();

                // Both message kinds keep the session alive, but only game
                // messages contain input for the simulation.
                if (decoded.kind == internal::ClientServerMessageKind::Game) {
                    pending.push_back({id, std::move(decoded.payload)});
                }
            }

            // Case 3: An admitted client is leaving the server.
        } else if (decoded.kind ==
                   internal::ClientServerMessageKind::Disconnect) {
            const auto id = decoded.client_id;
            if (id != 0 && id <= sessions.size() && sessions[id - 1].accepted) {
                DisconnectClient(server, id, pending);
            }
        }
    }

    // Check for and disconnect any clients that have not sent a heartbeat or
    // gameplay message within the timeout interval.

    const auto now = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < sessions.size(); ++index) {
        if (sessions[index].accepted && now - sessions[index].last_received >=
                                            internal::ConnectionTimeout) {
            const auto id = static_cast<ClientId>(index + 1);
            std::cerr << "Network server: client " << id << " timed out.\n";
            DisconnectClient(server, id, pending);
        }
    }
}

void NetworkServer::DisconnectClient(IServer &server, ClientId client,
                                     std::vector<ClientInput> &pending) {
    sessions[client - 1].accepted = false;
    sessions[client - 1].outgoing.clear();

    // Forget the join address and any inputs waiting for a simulation step
    // before the game removes this player.
    std::erase_if(clients, [client](const auto &entry) {
        return entry.second == client;
    });
    std::erase_if(pending, [client](const auto &input) {
        return input.client == client;
    });
    server.HandleDisconnect(world, client);
}

void NetworkServer::SendMessages() {

    // If it has been more than HeartbeatInterval since the last heartbeat,
    // queue a heartbeat message to each accepted client to keep the connection
    // alive.

    const auto now = std::chrono::steady_clock::now();
    if (now - last_heartbeat >= internal::HeartbeatInterval) {
        for (Session &session : sessions) {
            if (session.accepted) {
                session.outgoing.push_back(internal::EncodeHeartbeat());
            }
        }
        last_heartbeat = now;
    }

    // The messages queued by the current iteration, with their destinations.
    std::vector<AddressedMessage> batch;

    // For each session, move its queued outgoing messages into the batch
    // and clear the session's queue.
    for (Session &session : sessions) {
        for (NetworkMessage &message : session.outgoing) {
            batch.push_back({session.address, std::move(message)});
        }

        session.outgoing.clear();
    }

    // Now, our batch contains all messages queued for sending. We need
    // only transfer it to the communication thread, which owns the pipe and
    // will send the messages.

    // Append rather than replace, as the communication thread may not have
    // collected the previous iteration's messages yet.
    std::lock_guard lock(outgoing_mutex);
    outgoing.insert(outgoing.end(), std::make_move_iterator(batch.begin()),
                    std::make_move_iterator(batch.end()));
}

void NetworkServer::Communicate(std::stop_token stop) {
    // The shared pipe for all clients, created and destroyed on this thread.
    std::unique_ptr<MsgPipe> pipe;

    // The outgoing batch owned by this thread while sending.
    std::vector<AddressedMessage> sending;

    // The incoming batch owned by this thread while receiving.
    std::vector<AddressedMessage> received;

    while (!stop.stop_requested()) {
        // The requested local address, needed only until the pipe is created.
        // Nullopt if the pipe has already been created.
        std::optional<std::string> address;

        // Set up the pipe if we haven't already, and transfer any queued
        // outgoing messages to the sending batch. We only lock here so the main
        // thread can queue the next batch while this one is being sent.
        {
            std::lock_guard lock(outgoing_mutex);
            if (!pipe) {
                address = listen_address;
            }
            sending.swap(outgoing);
        }

        // If we have a requested address, create the pipe and bind it to that
        // address.
        if (address) {
            pipe = internal::MakePipe();
            pipe->Listen(*address);
        }

        // If the pipe is available, send the queued messages.
        if (pipe) {

            // For each message in the sending batch, send it through the pipe.
            for (const auto &message : sending) {
                if (!pipe->Send(message.address, message.message)) {
                    throw std::runtime_error(
                        "Network server could not send a message.");
                }
            }

            // Clear the sending batch after sending all messages, so we don't
            // resend them in the next iteration.
            sending.clear();

            // The current datagram and its sender, reused for each receive.
            AddressedMessage message;

            // Receive messages from the pipe until there are no more, or until
            // a stop is requested.
            while (!stop.stop_requested() &&
                   pipe->Receive(message.address, message.message)) {
                received.push_back(std::move(message));
            }

            // Transfer the received messages to the main thread's incoming
            // batch, so they can be processed in the next iteration.
            {
                std::lock_guard lock(incoming_mutex);
                incoming.insert(incoming.end(),
                                std::make_move_iterator(received.begin()),
                                std::make_move_iterator(received.end()));
            }

            // Clear the received batch after transferring, so we don't double
            // add them in the next iteration.
            received.clear();
        }

        // 1 millisecond sleep to avoid infinitely tight loop wasting CPU cycles
        // when there is no work to do.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

std::int32_t NetworkServer::run(IServer &server) {

    // First time setup.

    Vector2D gravity{};
    EventManager events;
    ServerContext context{world, events, *this, gravity};

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
                                   {world, events, *this, steps, parallel_for});
                           });
            events.DispatchEvents();

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
