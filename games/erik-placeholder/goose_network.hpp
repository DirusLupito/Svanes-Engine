#pragma once

#include <svanes/network/network_session.hpp>

#include <deque>
#include <optional>
#include <string>
#include <vector>

/**
 * The address and identity of one member of the game's fixed roster.
 * FIELDS:
 * - peer: The participant's shared id.
 * - host: The participant's numeric IPv4 address.
 * - port: The UDP port on which that participant listens.
 */
struct GoosePeerEndpoint {
    svanes::PeerId peer;
    std::string host;
    std::uint16_t port;
};

/**
 * Supplies the game session without tying it to a particular discovery method.
 * FIELDS:
 * - session: The nonzero session id shared by every process.
 * - local_peer: The participant controlled in this window.
 * - peers: The full roster, including the local participant's listening port.
 */
struct GooseNetworkConfiguration {
    svanes::SessionId session = 1;
    svanes::PeerId local_peer;
    std::vector<GoosePeerEndpoint> peers;
};

/**
 * Builds a local roster with ids 1 through player_count, using port_base + id.
 * @param local_peer The participant controlled by this process.
 * @param player_count The number of windows in the session.
 * @param port_base The port immediately below the first player's port.
 * @param session The shared session id.
 * @return A roster using 127.0.0.1 for every participant.
 * @throws std::invalid_argument for invalid ids, counts, or a port range exceeding 65535.
 */
GooseNetworkConfiguration MakeLoopbackGooseConfiguration(
    svanes::PeerId local_peer, std::uint32_t player_count,
    std::uint16_t port_base = 45000, svanes::SessionId session = 1);

/**
 * Message types owned by Erik's game.
 * - Ready: Confirms the roster, simulation step, and initial state.
 * - Input: Carries a player's input for a simulation tick.
 * - StateHash: Reports a state hash at a confirmed tick boundary.
 * - RosterStop: Announces a pause boundary and whether the sender is leaving.
 * - RosterPrepared: Confirms the common pause tick, world hash, and departure set.
 */
enum class GooseMessageType : svanes::MessageType {
    Ready = 1,
    Input = 2,
    StateHash = 3,
    RosterStop = 4,
    RosterPrepared = 5
};

/**
 * Connects a fixed roster and exchanges the game's readiness messages.
 * Each window independently announces readiness, then waits for every other
 * participant to announce the same starting state. No peer chooses the world
 * state for the others. Simulation ticks can begin once that agreement holds.
 *
 * NetworkSession handles delivery. This class handles the meaning of Ready
 * and queues gameplay messages for the simulation controller. Failed delivery
 * or disagreement leaves a status message for the game and prevents startup.
 */
class GooseNetwork final {
public:
    /**
     * Opens the local UDP port and configures direct routes to the other peers.
     * @param configuration The fixed session roster and local identity.
     * @throws std::invalid_argument for invalid or duplicate identities and addresses.
     */
    explicit GooseNetwork(GooseNetworkConfiguration configuration);

    /**
     * Requests startup using a hash of this window's initial simulation state.
     * @param initial_state_hash The hash captured before simulating tick zero.
     * @throws std::logic_error if an existing readiness request changes its state hash.
     */
    void RequestReady(std::uint64_t initial_state_hash);

    /**
     * Pumps delivery and processes readiness and failure reports each frame.
     * Gameplay messages remain queued until Receive() takes them.
     * @throws std::invalid_argument for malformed or unknown game messages.
     */
    void Update();

    /**
     * Queues a gameplay message for every peer after the roster is ready.
     * @param type The gameplay or roster-control message type.
     * @param payload The serialized game data.
     * @return Whether the message was accepted by the session.
     * @throws std::invalid_argument for a type outside gameplay and roster control.
     */
    bool Broadcast(GooseMessageType type, const svanes::NetworkMessage& payload);

    /**
     * Takes the next game message, including one from a peer that became ready sooner.
     * @param message Receives the sender, type, and game payload.
     * @return Whether a message was available and the session has not failed.
     */
    bool Receive(svanes::SessionMessage& message);

    /** @return The sorted roster, including the local participant. */
    std::span<const svanes::PeerId> Peers() const;

    /** @return The participant controlled in this window. */
    svanes::PeerId LocalPeer() const;

    /** @return Whether all participants have agreed on the starting state. */
    bool IsReady() const;

    /** @return Whether a peer failed or startup state disagreed. */
    bool HasFailed() const;

    /** @return A short connection status for the game's display or console. */
    std::string Status() const;

    /**
     * Adopts an agreed departure set and advances the roster revision.
     * Retired routes keep acknowledging retries while new traffic uses survivors.
     * @param departing The peers removed at the agreed simulation boundary.
     */
    void ApplyDepartures(std::span<const svanes::PeerId> departing);

    /** @return Whether all accepted outgoing messages have finished delivery. */
    bool OutgoingDrained() const;

    /** @return The revision attached to gameplay and roster-control messages. */
    std::uint64_t Revision() const;


private:
    /**
     * Validates and records a remote peer's readiness announcement.
     * @param message The received Ready message.
     */
    void ReadReady(const svanes::SessionMessage& message);

    std::unique_ptr<svanes::NetworkSession> session;
    std::vector<svanes::PeerId> peers;
    std::vector<std::optional<std::uint64_t>> ready_hashes;
    std::optional<std::uint64_t> initial_hash;
    std::uint64_t roster_hash = 0;
    bool ready_sent = false;
    std::string failure;
    std::deque<svanes::SessionMessage> messages;
    std::deque<svanes::SessionMessage> future_messages;
    std::uint64_t revision = 1;
    bool local_departed = false;
};
