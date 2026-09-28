#pragma once

#include <svanes/network/message_serialization.hpp>
#include <svanes/network/network_session.hpp>
#include <svanes/network/udp_msg_pipe.hpp>

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/** The session id every copy of the game uses. */
inline constexpr svanes::SessionId GooseSession = 1;

/** The port a new world listens on unless another is chosen. */
inline constexpr std::uint16_t GooseHostPort = 45000;

/** The port a joining process listens on unless another is chosen. */
inline constexpr std::uint16_t GooseJoinPort = 45001;

/** The sender id a joining process uses before it is assigned one. */
inline constexpr svanes::PeerId GooseUnassignedPeer{0xFFFFFFFF};

/**
 * The identity and address of one member of the world.
 * FIELDS:
 * - peer: The participant's shared id.
 * - address: The numeric IPv4 address and UDP port on which that participant listens.
 */
struct GoosePeerEndpoint {
    svanes::PeerId peer;
    svanes::UdpAddress address;
};

/**
 * A UDP pipe bound to a local port.
 * FIELDS:
 * - pipe: The bound pipe.
 * - port: The local port it listens on.
 */
struct GooseBoundPipe {
    std::unique_ptr<svanes::UdpMsgPipe> pipe;
    std::uint16_t port = 0;
};

/**
 * Binds a UDP pipe to a local port. The transport reuses addresses, so binding a
 * port another process already uses does not fail. Each copy of the game on one
 * computer needs its own port.
 * @param port The local port to listen on.
 * @return The bound pipe and its port.
 */
GooseBoundPipe BindGoosePipe(std::uint16_t port);

/**
 * Message types owned by Erik's game.
 * - Input: Carries a player's input for a simulation tick.
 * - StateHash: Reports a state hash at a confirmed tick boundary.
 * - RosterStop: Announces a pause boundary, whether the sender is leaving, and who it admits.
 * - RosterPrepared: Confirms the common pause tick, world hash, and roster change.
 * - JoinRequest: A contact from a process asking to join, with its rules hash.
 * - JoinPending: A contact reply saying the request is queued for the next roster change.
 * - JoinRejected: A contact reply giving the reason a request was refused.
 * - JoinAssigned: A contact reply with the joiner's id and the other members' addresses.
 * - SnapshotChunk: One piece of the world a sponsor sends to the player it admitted.
 */
enum class GooseMessageType : svanes::MessageType {
    Input = 2,
    StateHash = 3,
    RosterStop = 4,
    RosterPrepared = 5,
    JoinRequest = 6,
    JoinPending = 7,
    JoinRejected = 8,
    JoinAssigned = 9,
    SnapshotChunk = 10
};

/**
 * Writes a string as a 16-bit length followed by its bytes.
 * @param writer The message to append to.
 * @param text The text to write, at most 65535 bytes.
 * @throws std::length_error if the text is too long.
 */
void WriteGooseText(svanes::MessageWriter& writer, std::string_view text);

/**
 * @param reader The message positioned at text written by WriteGooseText.
 * @return The text.
 */
std::string ReadGooseText(svanes::MessageReader& reader);

/**
 * Writes a host and port.
 * @param writer The message to append to.
 * @param address The address to write.
 */
void WriteGooseAddress(svanes::MessageWriter& writer, const svanes::UdpAddress& address);

/**
 * @param reader The message positioned at an address written by WriteGooseAddress.
 * @return The address.
 * @throws std::invalid_argument for an empty host or a zero port.
 */
svanes::UdpAddress ReadGooseAddress(svanes::MessageReader& reader);

/**
 * Interprets an address passed along by another member. A loopback address only
 * means "on the relaying member's computer", so it is replaced by that member's
 * host as this process reaches it, keeping the port.
 * @param relayed The address as the relaying member announced it.
 * @param relay_host The host this process uses to reach the relaying member.
 * @return An address this process can reach.
 */
svanes::UdpAddress ResolveRelayedAddress(const svanes::UdpAddress& relayed, const std::string& relay_host);

/**
 * What a sponsor tells an admitted joiner.
 * FIELDS:
 * - local_peer: The id assigned to the joiner.
 * - sponsor: The member that admitted it, reached through the address the joiner contacted.
 * - revision: The roster revision that includes the joiner.
 * - peers: Every other member except the joiner and the sponsor.
 */
struct GooseAssignment {
    svanes::PeerId local_peer;
    svanes::PeerId sponsor;
    std::uint64_t revision = 0;
    std::vector<GoosePeerEndpoint> peers;
};

/**
 * @param assignment The assignment to send.
 * @return The JoinAssigned payload.
 */
svanes::NetworkMessage EncodeAssignment(const GooseAssignment& assignment);

/**
 * @param payload A JoinAssigned payload.
 * @return The assignment it carries.
 * @throws std::invalid_argument for malformed data.
 */
GooseAssignment DecodeAssignment(const svanes::NetworkMessage& payload);

/**
 * Connects this process to the world's other members and carries the game's messages.
 * A world starts with one member, peer 1, and grows as others join. NetworkSession
 * handles delivery. This class tags gameplay messages with the roster revision,
 * queues them for the simulation controller, splits snapshots into chunks, and
 * applies agreed roster changes to the session. Failed delivery leaves a status
 * message for the game.
 */
class GooseNetwork final {
public:
    /**
     * Starts a new world with this process as its only member, peer 1.
     * @param bound The pipe this process listens on.
     */
    explicit GooseNetwork(GooseBoundPipe bound);

    /**
     * Joins a world after admission, connecting to every member it was told about.
     * Loopback addresses in the assignment are resolved against the sponsor's host.
     * The complete world snapshot is then expected from the sponsor.
     * @param bound The pipe used during the join handshake.
     * @param sponsor_connection The pipe connection to the member that was contacted.
     * @param assignment The id, revision, and members received from the sponsor.
     */
    GooseNetwork(GooseBoundPipe bound, svanes::ConnectionId sponsor_connection,
                 const GooseAssignment& assignment);

    /**
     * Pumps delivery, reports failures, sorts incoming messages by revision,
     * collects snapshot chunks, and sends queued snapshot chunks.
     * @throws std::invalid_argument for malformed or unknown game messages.
     */
    void Update();

    /**
     * Queues a gameplay message for every member.
     * @param type The gameplay or roster-control message type.
     * @param payload The serialized game data.
     * @return Whether the message was accepted by the session.
     * @throws std::invalid_argument for a type outside gameplay and roster control.
     */
    bool Broadcast(GooseMessageType type, const svanes::NetworkMessage& payload);

    /**
     * Takes the next gameplay message for the current roster revision.
     * @param message Receives the sender, type, and game payload.
     * @return Whether a message was available and the session has not failed.
     */
    bool Receive(svanes::SessionMessage& message);

    /**
     * Splits a world snapshot into chunks and sends them to one member as capacity allows.
     * @param peer The admitted joiner.
     * @param snapshot The complete encoded snapshot.
     */
    void SendSnapshot(svanes::PeerId peer, const svanes::NetworkMessage& snapshot);

    /**
     * @return The complete snapshot once every chunk has arrived, returned only once.
     */
    std::optional<svanes::NetworkMessage> TakeSnapshot();

    /**
     * Takes the next contact or stranger message from the session.
     * @param stranger Receives the connection and message.
     * @return Whether one was available.
     */
    bool ReceiveStranger(svanes::StrangerMessage& stranger);

    /**
     * Sends one unreliable contact packet.
     * @param connection The destination.
     * @param type The join message type.
     * @param payload The serialized data.
     */
    void SendStranger(svanes::ConnectionId connection, GooseMessageType type,
                      const svanes::NetworkMessage& payload);

    /**
     * @param connection A connection known to the pipe.
     * @return The host and port behind it.
     */
    svanes::UdpAddress Address(svanes::ConnectionId connection) const;

    /**
     * @param connection A connection known to the pipe.
     * @return The active member using it, if any.
     */
    std::optional<svanes::PeerId> ActivePeerAt(svanes::ConnectionId connection) const;

    /**
     * @param peer A current remote member.
     * @return The address this process uses to reach it.
     * @throws std::invalid_argument if the peer is not a remote member.
     */
    svanes::UdpAddress PeerAddress(svanes::PeerId peer) const;

    /**
     * @return Every current member except this process, with the address this process uses for it.
     */
    std::vector<GoosePeerEndpoint> RemoteEndpoints() const;

    /**
     * Adopts an agreed roster change and advances the roster revision.
     * Retired routes keep acknowledging retries while new traffic uses the new roster.
     * @param departing The members removed at the agreed simulation boundary.
     * @param joining The members added at that boundary, with their addresses.
     */
    void ApplyRosterChange(std::span<const svanes::PeerId> departing,
                           std::span<const GoosePeerEndpoint> joining);

    /** @return The sorted roster, including the local participant. */
    std::span<const svanes::PeerId> Peers() const;

    /** @return The participant controlled in this window. */
    svanes::PeerId LocalPeer() const;

    /** @return The local UDP port others can join through. */
    std::uint16_t Port() const;

    /** @return Whether a peer failed or a message queue overflowed. */
    bool HasFailed() const;

    /** @return A short connection status for the game's display or console. */
    std::string Status() const;

    /** @return Whether all accepted outgoing messages have finished delivery. */
    bool OutgoingDrained() const;

    /** @return The revision attached to gameplay and roster-control messages. */
    std::uint64_t Revision() const;


private:
    /**
     * Snapshot chunks waiting for send capacity.
     * FIELDS:
     * - peer: The joiner receiving them.
     * - chunks: Every chunk payload in order.
     * - next: The index of the first chunk not yet accepted by the session.
     */
    struct OutgoingSnapshot {
        svanes::PeerId peer;
        std::vector<svanes::NetworkMessage> chunks;
        std::size_t next = 0;
    };

    /**
     * Records one received snapshot chunk.
     * @param message The SnapshotChunk message.
     */
    void ReadSnapshotChunk(const svanes::SessionMessage& message);

    std::unique_ptr<svanes::NetworkSession> session;
    svanes::UdpMsgPipe* pipe = nullptr;
    std::uint16_t port = 0;
    std::vector<svanes::PeerId> peers;
    std::string failure;
    std::deque<svanes::SessionMessage> messages;
    std::deque<svanes::SessionMessage> future_messages;
    std::uint64_t revision = 1;
    bool local_departed = false;
    std::vector<OutgoingSnapshot> outgoing_snapshots;
    bool awaiting_snapshot = false;
    std::optional<std::uint32_t> snapshot_chunk_count;
    std::map<std::uint32_t, std::vector<std::byte>> snapshot_chunks;
};

/**
 * Asks one member of an existing world to admit this process. Before admission
 * the process has no id and no session, so requests go out as contact packets
 * on a bare pipe, repeated until the member answers with an assignment or a
 * rejection. Admit() then hands the pipe to a GooseNetwork under the new id.
 */
class GooseJoin final {
public:
    /**
     * Binds a local port and prepares to contact a member.
     * @param entry The member's host and port.
     * @param port The local port this process listens on.
     * @param rules_hash This build's GooseRollback::RulesHash(), checked by the member.
     */
    GooseJoin(svanes::UdpAddress entry, std::uint16_t port, std::uint64_t rules_hash);

    /**
     * Resends the request when due and reads replies. Gives up after 15 seconds.
     * @throws std::invalid_argument for a malformed reply.
     */
    void Update();

    /** @return Whether an assignment has arrived. */
    bool IsAssigned() const;

    /** @return Whether the member rejected the request or never admitted it. */
    bool HasFailed() const;

    /** @return A short status for the console. */
    std::string Status() const;

    /**
     * Hands the pipe to a network session under the assigned id.
     * @return The joined network, which then expects the world snapshot.
     * @throws std::logic_error if no assignment has arrived.
     */
    std::unique_ptr<GooseNetwork> Admit();

private:
    GooseBoundPipe bound;
    svanes::UdpAddress entry;
    svanes::ConnectionId entry_connection;
    std::uint64_t rules_hash;
    std::chrono::steady_clock::time_point started_at;
    std::optional<std::chrono::steady_clock::time_point> last_request;
    bool answered = false;
    std::optional<GooseAssignment> assignment;
    std::string failure;
};
