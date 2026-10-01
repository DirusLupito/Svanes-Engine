#include "goose_session.hpp"
#include "goose_simulation.hpp"
#include "server_protocol.hpp"

#include <svanes/network/network_server.hpp>
#include <svanes/network/server_runtime.hpp>
#include <svanes/network/udp_msg_pipe.hpp>
#include <svanes/registry.hpp>
#include <svanes/timeline_system.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

namespace {

/**
 * One client's place on the server, with its own port and network thread.
 * FIELDS:
 * - client: The id of the client holding the slot, or empty while it is free.
 * - peer: The player that client controls.
 * - intent: The latest intent the client sent, used for every step until replaced.
 * - pending_dash: A dash the client sent that no step has used yet.
 * - clock: The latest clock request the client sent.
 * - last_heard: When the client was last heard from.
 */
struct Slot {
    std::optional<svanes::ClientId> client;
    svanes::PeerId peer{};
    GooseIntent intent{};
    float pending_dash = 0.0F;
    ClockRequest clock{};
    std::chrono::steady_clock::time_point last_heard{};
};

/**
 * @return One factory per slot, each opening a UDP pipe on that slot's port.
 */
std::vector<svanes::NetworkServer::PipeFactory> SlotPipes()
{
    std::vector<svanes::NetworkServer::PipeFactory> factories;
    for (std::uint32_t slot = 0; slot < GooseServerSlots; ++slot) {
        factories.push_back([slot] {
            return std::make_unique<svanes::UdpMsgPipe>(static_cast<std::uint16_t>(GooseServerFirstPort + slot));
        });
    }
    return factories;
}

/**
 * Sends finished worlds to clients on its own thread, so encoding and queueing
 * snapshots runs alongside the simulation instead of after it. The simulation
 * thread publishes each world it finishes; a world the thread has not yet sent
 * is replaced by a newer one, since clients only need the latest.
 */
class SnapshotBroadcaster final {
public:
    /**
     * Starts the broadcasting thread.
     * @param network The server whose slots the snapshots are sent through.
     */
    explicit SnapshotBroadcaster(svanes::NetworkServer& network)
        : network(network), thread([this](std::stop_token stop) { Run(stop); })
    {
    }

    /**
     * Hands a finished world to the broadcasting thread.
     * @param recipients The slots of every connected client.
     * @param roster Every player in ascending id order.
     * @param world Bytes from GooseSimulation::Save.
     */
    void Publish(std::vector<std::size_t> recipients, std::vector<svanes::PeerId> roster, svanes::NetworkMessage world)
    {
        {
            std::lock_guard lock(mutex);
            pending = World{std::move(recipients), std::move(roster), std::move(world)};
        }
        ready.notify_one();
    }

private:
    /**
     * A finished world waiting to be sent.
     * FIELDS:
     * - recipients: The slots of every connected client.
     * - roster: Every player in ascending id order.
     * - world: Bytes from GooseSimulation::Save.
     */
    struct World {
        std::vector<std::size_t> recipients;
        std::vector<svanes::PeerId> roster;
        svanes::NetworkMessage world;
    };

    /**
     * Waits for each published world, encodes it, and sends it to its recipients.
     * @param stop Ends the loop when the server shuts down.
     */
    void Run(std::stop_token stop)
    {
        while (true) {
            World next;
            {
                std::unique_lock lock(mutex);
                if (!ready.wait(lock, stop, [this] { return pending.has_value(); })) {
                    return;
                }
                next = std::move(*pending);
                pending.reset();
            }
            const svanes::NetworkMessage snapshot = EncodeSnapshot(next.roster, next.world);
            for (const std::size_t slot : next.recipients) {
                network.Send(slot, snapshot);
            }
        }
    }

    svanes::NetworkServer& network;
    std::mutex mutex;
    std::condition_variable_any ready;
    std::optional<World> pending;
    std::jthread thread;
};

/**
 * The authoritative goose world. Clients send input to their slots; every
 * step the server advances the one simulation everyone shares and hands the
 * result to its broadcaster, which sends it to each client on another thread.
 * While exactly one client is connected, that client's
 * clock request pauses or changes the speed of the world, as it would playing
 * alone peer-to-peer.
 */
class GooseServer final {
public:
    GooseServer()
        : simulation(world, {0.0F, svanes::PerSecondSquaredToPerTicSquared(GooseGravity)}, GooseTextures{}),
          network(SlotPipes()),
          broadcaster(network)
    {
    }

    /**
     * Serves clients until the process is stopped.
     * @throws std::invalid_argument if a client sends a malformed message.
     */
    void Run()
    {
        svanes::ServerRuntime::Run(
            network, GooseStepTics,
            [this](const svanes::ServerMessage& received) {
                Handle(received.session, DecodeGooseMessage(received.message, simulation));
            },
            [this] { Step(); }
        );
    }

private:
    /**
     * Acts on one message from a slot.
     * @param index The slot it arrived on.
     * @param message The decoded message.
     * @throws std::invalid_argument for a message only the server sends.
     */
    void Handle(std::size_t index, const GooseMessage& message)
    {
        Slot& slot = slots[index];
        const auto now = std::chrono::steady_clock::now();
        switch (message.type) {
        case GooseMessageType::Hello:
            if (!slot.client) {
                slot = Slot{.client = message.client, .peer = {next_peer++}, .last_heard = now};
                simulation.AddPlayer(slot.peer);
                std::cout << "Slot " << index << ": player " << slot.peer.value << " joined.\n";
            }
            if (*slot.client == message.client) {
                network.Send(index, EncodeWelcome(message.client, slot.peer));
            } else {
                network.Send(index, EncodeTaken(message.client));
            }
            return;
        case GooseMessageType::Input:
            if (slot.client) {
                slot.intent = message.intent;
                if (message.intent.dash != 0.0F) {
                    slot.pending_dash = message.intent.dash;
                }
                slot.clock = message.clock;
                slot.last_heard = now;
            }
            return;
        case GooseMessageType::Goodbye:
            if (slot.client) {
                Release(index, "left");
            }
            return;
        case GooseMessageType::Welcome:
        case GooseMessageType::Taken:
        case GooseMessageType::Snapshot:
            break;
        }
        throw std::invalid_argument("A client sent a message only the server sends.");
    }

    /**
     * Drops silent clients, advances the world by however many steps the world
     * clock allows, and sends every client the result.
     */
    void Step()
    {
        const auto now = std::chrono::steady_clock::now();
        for (std::size_t index = 0; index < slots.size(); ++index) {
            if (slots[index].client && now - slots[index].last_heard > GooseServerTimeout) {
                Release(index, "timed out");
            }
        }

        std::vector<std::size_t> players;
        for (std::size_t index = 0; index < slots.size(); ++index) {
            if (slots[index].client) {
                players.push_back(index);
            }
        }
        std::sort(players.begin(), players.end(), [this](std::size_t left, std::size_t right) {
            return slots[left].peer.value < slots[right].peer.value;
        });

        const bool alone = players.size() == 1;
        const ClockRequest request = alone ? slots[players.front()].clock : ClockRequest{};
        pending_tics += AdvanceWorldClock(clock, GooseStepTics, request, alone);
        while (pending_tics >= GooseStepTics) {
            std::vector<GooseIntent> inputs;
            for (const std::size_t index : players) {
                GooseIntent intent = slots[index].intent;
                intent.dash = slots[index].pending_dash;
                slots[index].pending_dash = 0.0F;
                inputs.push_back(intent);
            }
            simulation.Step(inputs);
            pending_tics -= GooseStepTics;
        }

        std::vector<svanes::PeerId> roster;
        for (const std::size_t index : players) {
            roster.push_back(slots[index].peer);
        }
        broadcaster.Publish(std::move(players), std::move(roster), simulation.Save());
    }

    /**
     * Frees a slot and removes its player from the world.
     * @param index The slot.
     * @param reason Why the player is leaving, for the console.
     */
    void Release(std::size_t index, const char* reason)
    {
        std::cout << "Slot " << index << ": player " << slots[index].peer.value << ' ' << reason << ".\n";
        simulation.RemovePlayer(slots[index].peer);
        slots[index] = Slot{};
    }

    svanes::Registry world;
    GooseSimulation simulation;
    svanes::NetworkServer network;
    SnapshotBroadcaster broadcaster;
    std::array<Slot, GooseServerSlots> slots{};
    svanes::Timeline clock;
    svanes::TicCount pending_tics = 0;
    std::uint32_t next_peer = 1;
};

}

int32_t main(int32_t argc, char**)
{
    if (argc > 1) {
        std::cerr << "The goose server takes no arguments. Clients find it on UDP ports "
            << GooseServerFirstPort << " to " << GooseServerFirstPort + GooseServerSlots - 1 << ".\n";
        return 1;
    }
    try {
        GooseServer server;
        std::cout << "Goose server listening on UDP ports " << GooseServerFirstPort << " to "
            << GooseServerFirstPort + GooseServerSlots - 1 << ".\n";
        server.Run();
    } catch (const std::exception& error) {
        std::cerr << "Goose server: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
