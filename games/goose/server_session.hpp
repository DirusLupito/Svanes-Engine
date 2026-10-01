#pragma once

#include "goose_session.hpp"
#include "goose_simulation.hpp"
#include "server_protocol.hpp"

#include <svanes/network/network_client.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/**
 * A world run by a goose server. The server alone advances the simulation;
 * this process sends its player's input and shows each snapshot it receives.
 *
 * Input is sampled every simulation step, so double taps time the same at any
 * frame rate, but is sent once per frame: a client running its loop at half
 * the rate sends half as many messages, and the server, stepping on its own
 * clock with each client's latest input, runs everyone else unaffected.
 *
 * To find a place, it says hello to the server's slots in order, moving on
 * when a slot is taken or does not answer, until one welcomes it.
 */
class ServerSession final : public GooseSession {
public:
    /**
     * Starts looking for a free slot on a server.
     * @param simulation The simulation that shows the server's snapshots.
     * @param host The server's numeric IPv4 address.
     */
    ServerSession(GooseSimulation& simulation, std::string host);

    /**
     * @throws std::runtime_error if every slot is taken or silent, if the
     * server stops sending snapshots, or if the server removed this player.
     */
    void Update(const svanes::FrameContext& frame, ClockRequest clock) override;
    bool IsRunning() const override;
    std::size_t PlayerCount() const override;
    svanes::PeerId LocalPeer() const override;
    void RequestLeave() override;
    bool CanClose() const override;
    std::string Status() const override;

private:
    /**
     * Opens a connection to a slot and greets it.
     * @param next_slot The slot to try.
     * @throws std::runtime_error if there are no slots left to try.
     */
    void ConnectToSlot(std::uint32_t next_slot);

    /**
     * Acts on one message from the server.
     * @param message The decoded message.
     * @throws std::runtime_error for a message only clients send.
     */
    void Handle(const GooseMessage& message);

    /**
     * Matches the simulation's players to a snapshot's roster and loads its world.
     * @param snapshot A Snapshot message.
     * @throws std::runtime_error if this player is no longer in the roster.
     */
    void ApplySnapshot(const GooseMessage& snapshot);

    GooseSimulation& simulation;
    std::string host;
    svanes::ClientId id;
    std::unique_ptr<svanes::NetworkClient> client;
    std::uint32_t slot = 0;
    std::chrono::steady_clock::time_point slot_deadline;
    std::chrono::steady_clock::time_point last_heard;
    std::optional<svanes::PeerId> local;
    std::vector<svanes::PeerId> roster;
    svanes::TicCount input_tics = 0;
    GooseIntent outgoing{};
    bool left = false;
};
